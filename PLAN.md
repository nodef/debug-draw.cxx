Write 5 examples for debug-draw in files 01-*.cxx, 02-*.cxx, ... in the `examples/` folder (currently empty). Below is a suggestive list of examples to write.

01-basic.cpp
Minimal setup with a dummy RenderInterface.
Basic initialization and shutdown.
Drawing a single line and point.

02-shapes.cpp
Demonstrates dd::sphere, dd::box, dd::aabb, dd::cone.
Uses different colors.

03-grid_axis.cpp
usage of dd::xzSquareGrid and dd::axisTriad.
Essential for debugging 3D scenes.

04-animation.cpp
Simulates a loop where objects move (update positions).
Formatting of dd::begin and dd::end in a loop context.

05-text.cpp
Demonstrates dd::projectedText and dd::screenText.
Shows how to annotate 3D objects.

Each of the above should use a different renderer, such as D3D11, D3D12, OpenGL, OpenGLES, and Vulkan. Write a short implementation for each renderer by implementing the RenderInterface.

Each file should be short, self-contained, and compileable (include all necessary headers and link all necessary libraries, using#pragma comment(lib, "...")).

Use https://github.com/glampert/debug-draw/tree/master/samples for reference
