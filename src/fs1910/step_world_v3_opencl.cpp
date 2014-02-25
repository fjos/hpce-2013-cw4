#include "heat.hpp"

#include <stdexcept>
#include <cmath>
#include <cstdint>
#include <memory>
#include <cstdio>
#include <fstream>
#include <streambuf>

#define __CL_ENABLE_EXCEPTIONS
#include "CL/cl.hpp"

namespace hpce
{
namespace fs1910
{
//! Reference world stepping program
/*! \param dt Amount to step the world by.  Note that large steps will be unstable.
    \param n Number of times to step the world
    \note Overall time increment will be n*dt
*/

void kernel_xy(uint32_t x, uint32_t y, uint32_t w, float inner, float outer, const float *world_state, const uint32_t *world_properties, float *buffer )
{
    unsigned index = y * w + x;

    if ((world_properties[index] & Cell_Fixed) || (world_properties[index] & Cell_Insulator))
    {
        // Do nothing, this cell never changes (e.g. a boundary, or an interior fixed-value heat-source)
        buffer[index] = world_state[index];
    }
    else
    {
        float contrib = inner;
        float acc = inner * world_state[index];

        // Cell above
        if (! (world_properties[index - w] & Cell_Insulator))
        {
            contrib += outer;
            acc += outer * world_state[index - w];
        }

        // Cell below
        if (! (world_properties[index + w] & Cell_Insulator))
        {
            contrib += outer;
            acc += outer * world_state[index + w];
        }

        // Cell left
        if (! (world_properties[index - 1] & Cell_Insulator))
        {
            contrib += outer;
            acc += outer * world_state[index - 1];
        }

        // Cell right
        if (! (world_properties[index + 1] & Cell_Insulator))
        {
            contrib += outer;
            acc += outer * world_state[index + 1];
        }

        // Scale the accumulate value by the number of places contributing to it
        float res = acc / contrib;
        // Then clamp to the range [0,1]
        res = std::min(1.0f, std::max(0.0f, res));
        buffer[index] = res;

    }
}

std::string LoadSource(const char *fileName)
{
    std::string baseDir = "src/fs1910";
    if (getenv("HPCE_CL_SRC_DIR"))
    {
        baseDir = getenv("HPCE_CL_SRC_DIR");
    }

    std::string fullName = baseDir + "/" + fileName;

    std::ifstream src(fullName, std::ios::in | std::ios::binary);
    if (!src.is_open())
        throw std::runtime_error("LoadSource : Couldn't load cl file from '" + fullName + "'.");

    return std::string(
               (std::istreambuf_iterator<char>(src)), // Node the extra brackets.
               std::istreambuf_iterator<char>()
           );
}

void StepWorldV3OpenCL(world_t &world, float dt, unsigned n)
{
    // Get platforms
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    if (platforms.size() == 0)
        throw std::runtime_error("No OpenCL platforms found.");

#ifdef DEBUG
    std::cerr << "Found " << platforms.size() << " platforms\n";
    for (unsigned i = 0; i < platforms.size(); i++)
    {
        std::string vendor = platforms[0].getInfo<CL_PLATFORM_VENDOR>();
        std::cerr << "  Platform " << i << " : " << vendor << "\n";
    }
#endif

    // select platforms
    int selectedPlatform = 0;
    if (getenv("HPCE_SELECT_PLATFORM"))
    {
        selectedPlatform = atoi(getenv("HPCE_SELECT_PLATFORM"));
    }

#ifdef DEBUG
    std::cerr << "Choosing platform " << selectedPlatform << "\n";
#endif

    cl::Platform platform = platforms.at(selectedPlatform);

    // Get devices
    std::vector<cl::Device> devices;
    platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);
    if (devices.size() == 0)
    {
        throw std::runtime_error("No opencl devices found.\n");
    }

#ifdef DEBUG
    std::cerr << "Found " << devices.size() << " devices\n";
    for (unsigned i = 0; i < devices.size(); i++)
    {
        std::string name = devices[i].getInfo<CL_DEVICE_NAME>();
        std::cerr << "  Device " << i << " : " << name << "\n";
    }
#endif

    // Select device
    int selectedDevice = 0;
    if (getenv("HPCE_SELECT_DEVICE"))
    {
        selectedDevice = atoi(getenv("HPCE_SELECT_DEVICE"));
    }
#ifdef DEBUG
    std::cerr << "Choosing device " << selectedDevice << "\n";
#endif

    cl::Device device = devices.at(selectedDevice);

    // Create context
    cl::Context context(devices);

    // Load kernel to string
    std::string kernelSource = LoadSource("step_world_v3_kernel.cl");

    // Load kernel to sources
    cl::Program::Sources sources;   // A vector of (data,length) pairs
    sources.push_back(std::make_pair(kernelSource.c_str(), kernelSource.size() + 1)); // push on our single string

    // Create program from context
    cl::Program program(context, sources);

#ifdef DEBUG
    try
    {
        program.build(devices);
    }
    catch (...)
    {
        for (unsigned i = 0; i < devices.size(); i++)
        {
            std::cerr << "Log for device " << devices[i].getInfo<CL_DEVICE_NAME>() << ":\n\n";
            std::cerr << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(devices[i]) << "\n\n";
        }
        throw;
    }
#else
    program.build(devices);
#endif

    size_t cbBuffer = 4 * world.w * world.h;
    cl::Buffer buffProperties(context, CL_MEM_READ_ONLY, cbBuffer);
    cl::Buffer buffState(context, CL_MEM_READ_ONLY, cbBuffer);
    cl::Buffer buffBuffer(context, CL_MEM_WRITE_ONLY, cbBuffer);

    cl::Kernel kernel(program, "kernel_xy");

    unsigned w = world.w, h = world.h;

    float outer = world.alpha * dt; // We spread alpha to other cells per time
    float inner = 1 - outer / 4;        // Anything that doesn't spread stays

    kernel.setArg(0, inner);
    kernel.setArg(1, outer);
    kernel.setArg(2, buffState);
    kernel.setArg(3, buffProperties);
    kernel.setArg(4, buffBuffer);

    cl::CommandQueue queue(context, device);

    queue.enqueueWriteBuffer(buffProperties, CL_TRUE, 0, cbBuffer, &world.properties[0]);

    // This is our temporary working space
    std::vector<float> buffer(w * h);



    for (unsigned t = 0; t < n; t++)
    {
        cl::Event evCopiedState;
        queue.enqueueWriteBuffer(buffState, CL_FALSE, 0, cbBuffer, &world.state[0], NULL, &evCopiedState);
        

        cl::NDRange offset(0, 0);               // Always start iterations at x=0, y=0
        cl::NDRange globalSize(w, h);   // Global size must match the original loops
        cl::NDRange localSize = cl::NullRange;  // We don't care about local size

        std::vector<cl::Event> kernelDependencies(1, evCopiedState);
        cl::Event evExecutedKernel;
        queue.enqueueNDRangeKernel(kernel, offset, globalSize, localSize, &kernelDependencies, &evExecutedKernel);


        std::vector<cl::Event> copyBackDependencies(1, evExecutedKernel);
        queue.enqueueReadBuffer(buffBuffer, CL_TRUE, 0, cbBuffer, &buffer[0], &copyBackDependencies);
        
        // All cells have now been calculated and placed in buffer, so we replace
        // the old state with the new state
        std::swap(world.state, buffer);
        // Swapping rather than assigning is cheaper: just a pointer swap
        // rather than a memcpy, so O(1) rather than O(w*h)

        world.t += dt; // We have moved the world forwards in time

    } // end of for(t...
}
}; // namespace fs1910


}; // namepspace hpce

int main(int argc, char *argv[])
{
    float dt = 0.1;
    unsigned n = 1;
    bool binary = false;

    if (argc > 1)
    {
        dt = strtof(argv[1], NULL);
    }
    if (argc > 2)
    {
        n = atoi(argv[2]);
    }
    if (argc > 3)
    {
        if (atoi(argv[3]))
            binary = true;
    }

    try
    {
        hpce::world_t world = hpce::LoadWorld(std::cin);
        std::cerr << "Loaded world with w=" << world.w << ", h=" << world.h << std::endl;

        std::cerr << "Stepping by dt=" << dt << " for n=" << n << std::endl;
        hpce::fs1910::StepWorldV3OpenCL(world, dt, n);

        hpce::SaveWorld(std::cout, world, binary);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception : " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

