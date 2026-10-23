#pragma once
// File offload (Linux). Guest memory that holds a game file's bytes as the APR read them (sound
// banks, collision and texture packs: 1.4-1.5 GB of Boletaria's 8 GB in RAM was still byte for byte
// its files') duplicates the file, which the disk holds anyway: on a 16 GB PC those pages could go
// and be read back from the file when touched.
//
// KYTY_FILE_OFFLOAD=probe (diagnostics): a MiB piece of an APR read's whole pages, unchanged since
// for 10 s (KYTY_FILE_OFFLOAD_AGE), is taken as the texture offload takes a texture
// (texture-offload.h: the page manager's hold on the guest view, no access in the backing view) and
// its bytes compared with the file's (the offload worker, through a spare view of the backing
// store); the equal ones stay taken, and each later touch gives one back and is counted, with how
// long after and which file: what dropping them would cost.
//
// KYTY_FILE_OFFLOAD=1 (the default with 20 GiB of RAM or less and vm.max_map_count 262144 or more):
// the equal pieces are dropped (a hole punched in the backing store's memfd); a touch reads them
// back from the file on the touching thread (a pread into the spare view: any thread can, none
// waits for the render thread), then gives them back. Files whose pieces are mostly touched within
// 10 s of being taken (streams read into the same buffers again and again) are left alone after a
// few.
//
// Here: the record of the APR reads (libAmpr) the scan picks pieces from
// (TextureCache::OffloadFiles, render thread).

#include "local-platform.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#endif

// Where it runs: Linux (the punched hole frees a piece's pages in every view; Windows has no such
// call for a section's pages).
#if defined(__linux__)
#define KYTY_FILE_OFFLOAD_SUPPORTED 1
#else
#define KYTY_FILE_OFFLOAD_SUPPORTED 0
#endif

namespace FileOffload {

enum class Mode { Off, Probe, Offload };

// The kernel's limit of a process's memory mappings (vm.max_map_count), 0 if unread.
inline uint64_t MaxMapCount() {
	uint64_t count = 0;
	if (FILE* file = std::fopen("/proc/sys/vm/max_map_count", "r")) {
		unsigned long long value = 0;
		if (std::fscanf(file, "%llu", &value) == 1) count = value;
		std::fclose(file);
	}
	return count;
}

// KYTY_FILE_OFFLOAD=probe|1|0 (default: on with 20 GiB of RAM or less, as the texture offload,
// where the kernel allows 262144 mappings or more). Each taken range splits the mappings of both
// views around it: in Demon's Souls the process's mappings went from 16-20 thousand to 48-52
// thousand, close to the old default limit of 65530 (Debian, older Ubuntu), past which a mapping or
// protection change fails and the emulator ends; Arch, Fedora 39+ and SteamOS allow far more.
inline Mode GetMode() {
#if KYTY_FILE_OFFLOAD_SUPPORTED
	static const Mode mode = [] {
		const char* value = std::getenv("KYTY_FILE_OFFLOAD");
		if (value == nullptr || value[0] == '\0') {
			const auto ram = LocalPlatform::PhysicalMemory();
			if (ram == 0 || ram > (uint64_t {20} << 30u)) return Mode::Off;
			if (const auto maps = MaxMapCount(); maps < (uint64_t {1} << 18u)) {
				std::printf("File offload: off (vm.max_map_count is %llu; with 262144 or more it "
				            "would drop about 1 GB of game file data from RAM: sysctl "
				            "vm.max_map_count=1048576)\n",
				            static_cast<unsigned long long>(maps));
				return Mode::Off;
			}
			return Mode::Offload;
		}
		if (value[0] == '0') return Mode::Off;
		return std::strcmp(value, "probe") == 0 ? Mode::Probe : Mode::Offload;
	}();
	return mode;
#else
	return Mode::Off;
#endif
}

inline bool Enabled() {
	return GetMode() != Mode::Off;
}

// The pieces: at most a MiB, a MiB-aligned guest span's whole pages of one read.
constexpr uint64_t PieceSize = uint64_t {1} << 20u;
constexpr uint64_t PageSize  = 4096;

struct Piece {
	uint64_t end    = 0;
	uint32_t file   = 0; // (an id of Files: 1, 2, ...)
	uint64_t offset = 0; // (the file's offset of the piece's first byte)
	std::chrono::steady_clock::time_point read;
};

struct Candidate {
	uint64_t begin = 0, end = 0, offset = 0;
	uint32_t file = 0;
};

struct Registry {
	std::mutex                mutex;
	std::map<uint64_t, Piece> pieces; // by guest start, not overlapping
	std::vector<std::string>  paths;  // id - 1 -> host path
	std::vector<int> fds; // id - 1 -> the file opened for reading (-1: not yet, -2: failed)
	// id - 1 -> its pieces taken, and touched within 10 s of it (QuickTouch): a file most of whose
	// pieces are (a stream read again and again into the same buffers, a table the game keeps
	// reading) is not taken any more.
	std::vector<uint32_t>                     taken, quick;
	std::unordered_map<std::string, uint32_t> ids;
	uint64_t                                  reads = 0, read_bytes = 0, hot_bytes = 0;
	std::chrono::steady_clock::time_point     start =
	    std::chrono::steady_clock::now(); // (the report's clock)
};

inline Registry& Get() {
	static Registry registry;
	return registry;
}

// (mutex held) The pieces over [begin, end) out of the record.
inline void EraseLocked(Registry& registry, uint64_t begin, uint64_t end) {
	auto it = registry.pieces.upper_bound(begin);
	if (it != registry.pieces.begin() && std::prev(it)->second.end > begin) --it;
	while (it != registry.pieces.end() && it->first < end)
		it = registry.pieces.erase(it);
}

// An APR read of `bytes` bytes of `host_path` at `offset` into guest memory at `destination` (any
// thread, after the copy). Pieces of older reads under it go.
inline void NoteRead(const std::string& host_path, uint64_t destination, uint64_t bytes,
                     uint64_t offset) {
	if (!Enabled() || bytes == 0) return;
	auto&           registry = Get();
	std::lock_guard lock(registry.mutex);
	EraseLocked(registry, destination, destination + bytes);
	registry.reads++;
	registry.read_bytes += bytes;
	const uint64_t begin = (destination + PageSize - 1) & ~(PageSize - 1);
	const uint64_t end   = (destination + bytes) & ~(PageSize - 1);
	if (end <= begin) return;
	auto [id, added] =
	    registry.ids.try_emplace(host_path, static_cast<uint32_t>(registry.paths.size() + 1));
	if (added) {
		registry.paths.push_back(host_path);
		registry.fds.push_back(-1);
		registry.taken.push_back(0);
		registry.quick.push_back(0);
	}
	const auto now = std::chrono::steady_clock::now();
	for (uint64_t at = begin; at < end;) {
		const uint64_t next = std::min(end, (at & ~(PieceSize - 1)) + PieceSize);
		registry.pieces.emplace(at, Piece {next, id->second, offset + (at - destination), now});
		at = next;
	}
}

// Render thread: up to `budget` bytes of pieces unchanged by reads for `age` (or longer), out of
// the record (a piece is taken once; a later read over its memory records it again).
inline std::vector<Candidate> Take(std::chrono::steady_clock::duration age, uint64_t budget) {
	auto&                  registry = Get();
	std::vector<Candidate> taken;
	const auto             now = std::chrono::steady_clock::now();
	std::lock_guard        lock(registry.mutex);
	uint64_t               bytes = 0;
	for (auto it = registry.pieces.begin(); it != registry.pieces.end() && bytes < budget;) {
		if (now - it->second.read < age) {
			++it;
			continue;
		}
		const auto file = it->second.file - 1;
		if (registry.quick[file] >= 4 && registry.quick[file] * 4 >= registry.taken[file]) {
			registry.hot_bytes += it->second.end - it->first;
		} else {
			taken.push_back({it->first, it->second.end, it->second.offset, it->second.file});
			bytes += it->second.end - it->first;
			registry.taken[file]++;
		}
		it = registry.pieces.erase(it);
	}
	return taken;
}

// A taken piece of file `file` touched within 10 s of being taken (any thread; TextureOffload's
// lock held).
inline void QuickTouch(uint32_t file) {
	auto&           registry = Get();
	std::lock_guard lock(registry.mutex);
	if (file != 0 && file <= registry.quick.size()) registry.quick[file - 1]++;
}

// The file of id `file` opened for reading (kept open: a touch reads from it in a fault handler),
// -1 if it cannot be. First called from the offload worker before the piece is dropped.
inline int Fd(uint32_t file) {
#if KYTY_FILE_OFFLOAD_SUPPORTED
	auto&           registry = Get();
	std::lock_guard lock(registry.mutex);
	if (file == 0 || file > registry.fds.size()) return -1;
	int& fd = registry.fds[file - 1];
	if (fd == -1) {
		fd = open(registry.paths[file - 1].c_str(), O_RDONLY | O_CLOEXEC);
		if (fd < 0) fd = -2;
	}
	return fd >= 0 ? fd : -1;
#else
	(void)file;
	return -1;
#endif
}

// The file's name (no directory), for the report.
inline std::string Name(uint32_t file) {
	auto&           registry = Get();
	std::lock_guard lock(registry.mutex);
	if (file == 0 || file > registry.paths.size()) return "?";
	const auto& path  = registry.paths[file - 1];
	const auto  slash = path.find_last_of("/\\");
	return slash == std::string::npos ? path : path.substr(slash + 1);
}

// Pieces recorded and not taken yet, the reads so far, the bytes of pieces left alone (their files'
// QuickTouch) and the files that are.
inline void Pending(uint64_t* bytes, uint64_t* reads, uint64_t* read_bytes, uint64_t* hot_bytes,
                    uint64_t* hot_files) {
	auto&           registry = Get();
	std::lock_guard lock(registry.mutex);
	*bytes = 0;
	for (const auto& [begin, piece]: registry.pieces)
		*bytes += piece.end - begin;
	*reads      = registry.reads;
	*read_bytes = registry.read_bytes;
	*hot_bytes  = registry.hot_bytes;
	*hot_files  = 0;
	for (size_t i = 0; i < registry.quick.size(); ++i)
		*hot_files += registry.quick[i] >= 4 && registry.quick[i] * 4 >= registry.taken[i] ? 1 : 0;
}

} // namespace FileOffload
