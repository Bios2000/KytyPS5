// Subgroup layout probe: for 64-invocation compute workgroups on 32-lane subgroups, does the driver put
// gl_LocalInvocationIndex 0-31 in subgroup 0 and 32-63 in subgroup 1, each in order? KytyPS5's cooperative halves
// (and the lane-local path's ADD_TID lane) assume it; the Vulkan spec does not promise it.
// Builds on Linux (clang++ probe.cpp -ldl) and Windows (clang-cl); loads the Vulkan loader at run time.
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
static void* OpenLoader() { return reinterpret_cast<void*>(LoadLibraryA("vulkan-1.dll")); }
static void* Symbol(void* lib, const char* name) {
	return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(lib), name));
}
#else
#include <dlfcn.h>
static void* OpenLoader() {
	void* lib = dlopen("libvulkan.so.1", RTLD_NOW);
	return lib != nullptr ? lib : dlopen("libvulkan.so", RTLD_NOW);
}
static void* Symbol(void* lib, const char* name) { return dlsym(lib, name); }
#endif

#include "shaders.h"

#define FUNCS(X)                                                                                                       \
	X(vkCreateInstance) X(vkEnumeratePhysicalDevices) X(vkGetPhysicalDeviceProperties2)                                \
	X(vkGetPhysicalDeviceQueueFamilyProperties) X(vkGetPhysicalDeviceMemoryProperties) X(vkCreateDevice)               \
	X(vkGetDeviceQueue) X(vkCreateBuffer) X(vkGetBufferMemoryRequirements) X(vkAllocateMemory) X(vkBindBufferMemory)    \
	X(vkMapMemory) X(vkCreateImage) X(vkGetImageMemoryRequirements) X(vkBindImageMemory) X(vkCreateImageView)          \
	X(vkCreateSampler) X(vkCreateDescriptorSetLayout) X(vkCreatePipelineLayout) X(vkCreateDescriptorPool)              \
	X(vkAllocateDescriptorSets) X(vkUpdateDescriptorSets) X(vkCreateShaderModule) X(vkCreateComputePipelines)          \
	X(vkCreateCommandPool) X(vkAllocateCommandBuffers) X(vkBeginCommandBuffer) X(vkCmdPipelineBarrier)                 \
	X(vkCmdBindPipeline) X(vkCmdBindDescriptorSets) X(vkCmdDispatch) X(vkEndCommandBuffer) X(vkQueueSubmit)            \
	X(vkQueueWaitIdle) X(vkResetCommandBuffer) X(vkCmdFillBuffer) X(vkDestroyPipeline)

#define DECLARE(name) static PFN_##name name;
FUNCS(DECLARE)

static FILE* g_out = nullptr;
static void Print(const char* format, ...) {
	char    text[4096];
	va_list args;
	va_start(args, format);
	std::vsnprintf(text, sizeof(text), format, args);
	va_end(args);
	std::fputs(text, stdout);
	if (g_out != nullptr) std::fputs(text, g_out);
}

#define CHECK(call)                                                                                                    \
	do {                                                                                                               \
		VkResult result_ = (call);                                                                                     \
		if (result_ != VK_SUCCESS) {                                                                                   \
			Print("FAILED: %s = %d\n", #call, static_cast<int>(result_));                                              \
			std::exit(1);                                                                                              \
		}                                                                                                              \
	} while (0)

static uint32_t MemoryType(const VkPhysicalDeviceMemoryProperties& props, uint32_t bits, VkMemoryPropertyFlags want) {
	for (uint32_t i = 0; i < props.memoryTypeCount; i++)
		if ((bits & (1u << i)) != 0 && (props.memoryTypes[i].propertyFlags & want) == want) return i;
	Print("FAILED: no memory type 0x%x\n", want);
	std::exit(1);
}

int main(int argc, char** argv) {
	g_out = std::fopen("subgroup_probe.txt", "w");
	uint32_t gpu_index = argc > 1 ? static_cast<uint32_t>(std::atoi(argv[1])) : 0u;
	void*    lib       = OpenLoader();
	if (lib == nullptr) {
		Print("FAILED: Vulkan loader not found\n");
		return 1;
	}
	auto get_instance_proc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(Symbol(lib, "vkGetInstanceProcAddr"));
	vkCreateInstance       = reinterpret_cast<PFN_vkCreateInstance>(get_instance_proc(nullptr, "vkCreateInstance"));

	VkApplicationInfo app {VK_STRUCTURE_TYPE_APPLICATION_INFO};
	app.pApplicationName = "subgroup_probe";
	app.apiVersion       = VK_API_VERSION_1_3;
	VkInstanceCreateInfo instance_info {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
	instance_info.pApplicationInfo = &app;
	VkInstance instance            = VK_NULL_HANDLE;
	CHECK(vkCreateInstance(&instance_info, nullptr, &instance));
#define LOAD(name) name = reinterpret_cast<PFN_##name>(get_instance_proc(instance, #name));
	FUNCS(LOAD)

	uint32_t count = 0;
	CHECK(vkEnumeratePhysicalDevices(instance, &count, nullptr));
	std::vector<VkPhysicalDevice> gpus(count);
	CHECK(vkEnumeratePhysicalDevices(instance, &count, gpus.data()));
	if (gpu_index >= count) {
		Print("FAILED: GPU %u of %u\n", gpu_index, count);
		return 1;
	}
	VkPhysicalDevice gpu = gpus[gpu_index];

	VkPhysicalDeviceDriverProperties          driver {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
	VkPhysicalDeviceSubgroupSizeControlProperties size_control {
	    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES};
	VkPhysicalDeviceSubgroupProperties subgroup {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
	driver.pNext       = &size_control;
	size_control.pNext = &subgroup;
	VkPhysicalDeviceProperties2 props {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
	props.pNext = &driver;
	vkGetPhysicalDeviceProperties2(gpu, &props);
	Print("GPU %u of %u: %s (vendor 0x%04x device 0x%04x)\n", gpu_index, count, props.properties.deviceName,
	      props.properties.vendorID, props.properties.deviceID);
	Print("Driver: %s %s\n", driver.driverName, driver.driverInfo);
	Print("Subgroups: default %u, control %u-%u, required size in compute %s\n", subgroup.subgroupSize,
	      size_control.minSubgroupSize, size_control.maxSubgroupSize,
	      (size_control.requiredSubgroupSizeStages & VK_SHADER_STAGE_COMPUTE_BIT) ? "yes" : "no");

	uint32_t families = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(gpu, &families, nullptr);
	std::vector<VkQueueFamilyProperties> family_props(families);
	vkGetPhysicalDeviceQueueFamilyProperties(gpu, &families, family_props.data());
	uint32_t family = 0;
	while (family < families && (family_props[family].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0) family++;

	VkPhysicalDeviceVulkan13Features features13 {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
	features13.subgroupSizeControl  = VK_TRUE;
	features13.computeFullSubgroups = VK_TRUE;
	float                   priority = 1.0f;
	VkDeviceQueueCreateInfo queue_info {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
	queue_info.queueFamilyIndex = family;
	queue_info.queueCount       = 1;
	queue_info.pQueuePriorities = &priority;
	VkDeviceCreateInfo device_info {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
	device_info.pNext                = &features13;
	device_info.queueCreateInfoCount = 1;
	device_info.pQueueCreateInfos    = &queue_info;
	VkDevice device                  = VK_NULL_HANDLE;
	CHECK(vkCreateDevice(gpu, &device_info, nullptr, &device));
	VkQueue queue = VK_NULL_HANDLE;
	vkGetDeviceQueue(device, family, 0, &queue);

	VkPhysicalDeviceMemoryProperties memory_props {};
	vkGetPhysicalDeviceMemoryProperties(gpu, &memory_props);

	constexpr uint32_t Groups = 16; // 4 x 4 workgroups
	constexpr VkDeviceSize BufferSize = Groups * 64 * 4 * sizeof(uint32_t);
	VkBufferCreateInfo buffer_info {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	buffer_info.size  = BufferSize;
	buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	VkBuffer buffer   = VK_NULL_HANDLE;
	CHECK(vkCreateBuffer(device, &buffer_info, nullptr, &buffer));
	VkMemoryRequirements requirements {};
	vkGetBufferMemoryRequirements(device, buffer, &requirements);
	VkMemoryAllocateInfo alloc {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	alloc.allocationSize  = requirements.size;
	alloc.memoryTypeIndex = MemoryType(memory_props, requirements.memoryTypeBits,
	                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	VkDeviceMemory buffer_memory = VK_NULL_HANDLE;
	CHECK(vkAllocateMemory(device, &alloc, nullptr, &buffer_memory));
	CHECK(vkBindBufferMemory(device, buffer, buffer_memory, 0));
	void* mapped = nullptr;
	CHECK(vkMapMemory(device, buffer_memory, 0, BufferSize, 0, &mapped));

	VkImageCreateInfo image_info {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
	image_info.imageType   = VK_IMAGE_TYPE_2D;
	image_info.format      = VK_FORMAT_R8G8B8A8_UNORM;
	image_info.extent      = {8, 8, 1};
	image_info.mipLevels   = 1;
	image_info.arrayLayers = 1;
	image_info.samples     = VK_SAMPLE_COUNT_1_BIT;
	image_info.tiling      = VK_IMAGE_TILING_OPTIMAL;
	image_info.usage       = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	VkImage image          = VK_NULL_HANDLE;
	CHECK(vkCreateImage(device, &image_info, nullptr, &image));
	vkGetImageMemoryRequirements(device, image, &requirements);
	alloc.allocationSize  = requirements.size;
	alloc.memoryTypeIndex = MemoryType(memory_props, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VkDeviceMemory image_memory = VK_NULL_HANDLE;
	CHECK(vkAllocateMemory(device, &alloc, nullptr, &image_memory));
	CHECK(vkBindImageMemory(device, image, image_memory, 0));
	VkImageViewCreateInfo view_info {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
	view_info.image            = image;
	view_info.viewType         = VK_IMAGE_VIEW_TYPE_2D;
	view_info.format           = VK_FORMAT_R8G8B8A8_UNORM;
	view_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
	VkImageView view           = VK_NULL_HANDLE;
	CHECK(vkCreateImageView(device, &view_info, nullptr, &view));
	VkSamplerCreateInfo sampler_info {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
	sampler_info.magFilter = VK_FILTER_LINEAR;
	sampler_info.minFilter = VK_FILTER_LINEAR;
	VkSampler sampler      = VK_NULL_HANDLE;
	CHECK(vkCreateSampler(device, &sampler_info, nullptr, &sampler));

	VkDescriptorSetLayoutBinding bindings[3] = {
	    {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
	    {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
	    {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}};
	VkDescriptorSetLayoutCreateInfo set_layout_info {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	set_layout_info.bindingCount = 3;
	set_layout_info.pBindings    = bindings;
	VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
	CHECK(vkCreateDescriptorSetLayout(device, &set_layout_info, nullptr, &set_layout));
	VkPipelineLayoutCreateInfo layout_info {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	layout_info.setLayoutCount = 1;
	layout_info.pSetLayouts    = &set_layout;
	VkPipelineLayout layout    = VK_NULL_HANDLE;
	CHECK(vkCreatePipelineLayout(device, &layout_info, nullptr, &layout));
	VkDescriptorPoolSize pool_sizes[3] = {{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
	                                      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
	                                      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}};
	VkDescriptorPoolCreateInfo pool_info {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
	pool_info.maxSets       = 1;
	pool_info.poolSizeCount = 3;
	pool_info.pPoolSizes    = pool_sizes;
	VkDescriptorPool pool   = VK_NULL_HANDLE;
	CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &pool));
	VkDescriptorSetAllocateInfo set_alloc {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	set_alloc.descriptorPool     = pool;
	set_alloc.descriptorSetCount = 1;
	set_alloc.pSetLayouts        = &set_layout;
	VkDescriptorSet set          = VK_NULL_HANDLE;
	CHECK(vkAllocateDescriptorSets(device, &set_alloc, &set));
	VkDescriptorBufferInfo buffer_desc {buffer, 0, BufferSize};
	VkDescriptorImageInfo  storage_desc {VK_NULL_HANDLE, view, VK_IMAGE_LAYOUT_GENERAL};
	VkDescriptorImageInfo  sampled_desc {sampler, view, VK_IMAGE_LAYOUT_GENERAL};
	VkWriteDescriptorSet   writes[3] {};
	for (uint32_t i = 0; i < 3; i++) {
		writes[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].dstSet          = set;
		writes[i].dstBinding      = i;
		writes[i].descriptorCount = 1;
		writes[i].descriptorType  = bindings[i].descriptorType;
	}
	writes[0].pBufferInfo = &buffer_desc;
	writes[1].pImageInfo  = &storage_desc;
	writes[2].pImageInfo  = &sampled_desc;
	vkUpdateDescriptorSets(device, 3, writes, 0, nullptr);

	VkCommandPoolCreateInfo command_pool_info {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
	command_pool_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	command_pool_info.queueFamilyIndex = family;
	VkCommandPool command_pool         = VK_NULL_HANDLE;
	CHECK(vkCreateCommandPool(device, &command_pool_info, nullptr, &command_pool));
	VkCommandBufferAllocateInfo cmd_alloc {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
	cmd_alloc.commandPool        = command_pool;
	cmd_alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmd_alloc.commandBufferCount = 1;
	VkCommandBuffer cmd          = VK_NULL_HANDLE;
	CHECK(vkAllocateCommandBuffers(device, &cmd_alloc, &cmd));

	// The image to GENERAL once.
	VkCommandBufferBeginInfo begin {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	CHECK(vkBeginCommandBuffer(cmd, &begin));
	VkImageMemoryBarrier to_general {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
	to_general.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
	to_general.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
	to_general.newLayout           = VK_IMAGE_LAYOUT_GENERAL;
	to_general.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	to_general.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	to_general.image               = image;
	to_general.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0,
	                     nullptr, 1, &to_general);
	CHECK(vkEndCommandBuffer(cmd));
	VkSubmitInfo submit {VK_STRUCTURE_TYPE_SUBMIT_INFO};
	submit.commandBufferCount = 1;
	submit.pCommandBuffers    = &cmd;
	CHECK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));
	CHECK(vkQueueWaitIdle(queue));

	struct Request {
		const char* name;
		uint32_t    size;  // 0: none
		bool        full;  // REQUIRE_FULL_SUBGROUPS
	};
	const Request requests[] = {{"required 32 (KytyPS5's way)", 32, false},
	                            {"required 32 + full subgroups", 32, true},
	                            {"no required size", 0, false}};
	uint32_t mismatched_runs = 0;
	for (const auto& shader: kShaders) {
		VkShaderModuleCreateInfo module_info {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
		module_info.codeSize = shader.words * sizeof(uint32_t);
		module_info.pCode    = shader.code;
		VkShaderModule module = VK_NULL_HANDLE;
		CHECK(vkCreateShaderModule(device, &module_info, nullptr, &module));
		for (const auto& request: requests) {
			if (request.size != 0 && (request.size < size_control.minSubgroupSize || request.size > size_control.maxSubgroupSize))
				continue;
			VkPipelineShaderStageRequiredSubgroupSizeCreateInfo required {
			    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO};
			required.requiredSubgroupSize = request.size;
			VkComputePipelineCreateInfo pipeline_info {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
			pipeline_info.stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			pipeline_info.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
			pipeline_info.stage.module = module;
			pipeline_info.stage.pName  = "main";
			if (request.size != 0) pipeline_info.stage.pNext = &required;
			if (request.full) pipeline_info.stage.flags = VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT;
			pipeline_info.layout = layout;
			VkPipeline pipeline  = VK_NULL_HANDLE;
			CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline));

			CHECK(vkResetCommandBuffer(cmd, 0));
			CHECK(vkBeginCommandBuffer(cmd, &begin));
			vkCmdFillBuffer(cmd, buffer, 0, BufferSize, 0xffffffffu);
			VkMemoryBarrier fill_done {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
			fill_done.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			fill_done.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
			                     &fill_done, 0, nullptr, 0, nullptr);
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &set, 0, nullptr);
			vkCmdDispatch(cmd, 4, 4, 1);
			VkMemoryBarrier to_host {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
			to_host.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			to_host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
			vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &to_host, 0,
			                     nullptr, 0, nullptr);
			CHECK(vkEndCommandBuffer(cmd));
			CHECK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));
			CHECK(vkQueueWaitIdle(queue));

			// Per invocation (by gl_LocalInvocationIndex): the lane KytyPS5 assumes (index & 63) against the one the
			// driver gave it (gl_SubgroupID * size + gl_SubgroupInvocationID).
			const auto* data       = static_cast<const uint32_t*>(mapped);
			uint32_t    size_seen  = 0, mismatches = 0, first_group = UINT32_MAX;
			std::string map;
			for (uint32_t group = 0; group < Groups; group++)
				for (uint32_t index = 0; index < 64; index++) {
					const uint32_t* record = data + (group * 64 + index) * 4;
					const uint32_t  size   = record[2];
					size_seen              = size;
					const uint32_t  lane   = record[0] * size + record[1];
					if (lane != index) {
						mismatches++;
						if (first_group == UINT32_MAX) first_group = group;
					}
				}
			if (first_group != UINT32_MAX) {
				// That group's layout: for each subgroup lane in order, the gl_LocalInvocationIndex it holds.
				std::vector<int> held(64, -1);
				for (uint32_t index = 0; index < 64; index++) {
					const uint32_t* record = data + (first_group * 64 + index) * 4;
					const uint32_t  lane   = record[0] * record[2] + record[1];
					if (lane < 64) held[lane] = static_cast<int>(index);
				}
				for (uint32_t lane = 0; lane < 64; lane++) map += std::to_string(held[lane]) + (lane % 16 == 15 ? "\n      " : " ");
			}
			Print("%-18s %-30s subgroup size %2u: %s", shader.name, request.name, size_seen,
			      mismatches == 0 ? "linear (index = subgroup * size + lane)\n" : "NOT LINEAR");
			if (mismatches != 0) {
				mismatched_runs++;
				Print(", %u of %u invocations; group %u, lanes in order hold the indices:\n      %s\n", mismatches,
				      Groups * 64, first_group, map.c_str());
			}
			vkDestroyPipeline(device, pipeline, nullptr);
		}
	}
	Print("\n%s\n", mismatched_runs == 0 ? "Result: every layout linear." : "Result: some layouts NOT linear (see above).");
	if (g_out != nullptr) std::fclose(g_out);
	return 0;
}
