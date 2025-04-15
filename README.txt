This project implements a node-based shader editor similar to Blender's Shader Graph, allowing you to create custom shaders in a visual, drag-and-drop manner. It leverages DirectX 11 for rendering, STB Image for image loading, and includes nodes for manipulating textures (like adjusting brightness and contrast).

📌 Project Overview
This project enables real-time shader editing using a node-based interface where you can build shaders visually. Here’s an overview of what has been implemented:

Background Grid & Layout: A working grid layout similar to shader graph tools.

Properties Panel: A properties panel for controlling node properties (e.g., sliders for brightness/contrast).

Node System: A base node class from which various nodes (Input, Output, Brightness) are derived.

Pin System: Nodes are connected via pins, with the ability to link them together.

Shader Compilation: Brightness and contrast nodes are connected to an HLSL shader, compiled, and applied to a texture in real-time using DirectX 11.

Image Manipulation: Brightness adjustment applied to images using a pixel shader, with real-time updates in the editor.

The project also uses STB Image for texture loading and DirectX 11 for rendering the texture with the applied shader.

🛠️ Features Implemented
Node Editor Interface:

Background grid for organizing nodes.

Layout management to organize and visualize nodes.

Buttons to create different types of nodes (input, output, brightness).

Node System:

A base node class to define the common structure of nodes.

Input, Output, Brightness nodes are implemented as subclasses of the base node class.

Pin System:

Each node has input and output pins.

Pins are used to connect nodes to each other, establishing data flow.

Link System:

Nodes are linked by dragging pins between them.

Links are visualized to show how data flows through the nodes.

Shader Compilation:

HLSL shaders for brightness and contrast are dynamically compiled at runtime.

A constant buffer is used to transfer parameters (e.g., brightness, contrast) to the shader.

Texture Manipulation:

Brightness and contrast are modified in real-time on an input texture using a pixel shader.

STB Image Support:

Image files (e.g., .png, .jpg) are loaded and used as input textures in the node editor.

DirectX 11 Integration:

Uses DirectX 11 to render the modified texture with the applied shader.

Shader parameters like brightness and contrast are dynamically updated.

🧩 How It Works - Step-by-Step
Initialize the Grid and Layout:

The background grid is set up in the window to organize the layout of nodes.

A panel is used for displaying properties (e.g., sliders to control brightness/contrast).

Create Nodes:

Users can add new nodes (Input, Output, Brightness) via buttons on the properties panel.

The input node allows users to load and display a texture.

The brightness node modifies the brightness of the input texture.

The output node displays the final result.

Link Nodes:

Users can connect nodes via pins.

The data (e.g., brightness value) is passed through these connections from one node to another.

Compile Shader:

The pixel shader for brightness/contrast adjustment is dynamically compiled using HLSL.

The shader receives data (e.g., brightness, contrast) from the constant buffer and applies it to the input texture.

Render Image:

After the shader is compiled and linked, the image is rendered to the screen using DirectX 11.

🚀 How to Use the Editor
Run the Application:

Compile the project and run the executable.

Load a Texture:

Click on the Input Node button to load a texture (e.g., a .png or .jpg file) from your system.

Adjust Brightness/Contrast:

Click on the Brightness Node to open the properties panel.

Use the Slider to adjust the brightness and contrast of the texture.

Link Nodes:

Click on the pins of the Input Node and Brightness Node to link them together.

Similarly, connect the Brightness Node to the Output Node to visualize the results.

View the Result:

The final adjusted texture will be displayed in the Output Node after all the nodes are linked and parameters are set.

🧰 Build Instructions
Prerequisites:
C++ Compiler: Ensure you have a C++ compiler like MSVC (Microsoft Visual C++) installed.

DirectX SDK: Make sure you have the DirectX SDK set up for building DirectX 11 applications.

STB Image Library: The project uses the STB Image library to load textures.

Steps to Build:
Clone the Repository:

bash
Copy
Edit
git clone <repo_url>
cd <repo_directory>
Set Up Visual Studio:

Open the project in Visual Studio (or your preferred IDE).

Ensure you are targeting a compatible version of the Windows SDK and DirectX SDK.

Build the Project:

In Visual Studio, click Build > Build Solution.

Run the Application:

Once the build completes, run the executable.

A window will open where you can interact with the node editor.

⚙️ Libraries Used
STB Image: Used for loading textures (images) in various formats like PNG, JPG, etc.

Link: STB Image

DirectX 11: Used for rendering the scene and applying shaders to textures.

Link: DirectX 11

Note: Brightness Node is not functional due to some errors!!!
