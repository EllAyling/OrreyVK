glslangvalidator -V orbit.vert -o orbit.vert.spv --target-env vulkan1.2
glslangvalidator -V orbit.frag -o orbit.frag.spv --target-env vulkan1.2

glslangvalidator -V planets.vert -o planets.vert.spv --target-env vulkan1.2
glslangvalidator -V planets.comp -o planets.comp.spv --target-env vulkan1.2
glslangvalidator -V planets.frag -o planets.frag.spv --target-env vulkan1.2

glslangvalidator -V skysphere.vert -o skysphere.vert.spv --target-env vulkan1.2
glslangvalidator -V skysphere.frag -o skysphere.frag.spv --target-env vulkan1.2