# rE:miniCO

> *re·mini·co* — from "reminisce" and "eco": a small ecosystem of your own, reimagined.

A 3D renderer and procedural ecosystem simulator built in C++20 and Vulkan 1.3. Terrain, vegetation, animals, water, and volumetric clouds, all simulated and rendered in a single cohesive scene, driven by a multithreaded engine and a suite of GLSL compute shaders.


---


## Design Philosophy

The simulation's design pattern-recognizes the emergent nature of real ecosystem to derive fundamental rules of plants and animals with minimal discretion and archetyping. Hence, it maximizes expressiveness, offering infinitely self-generating content in form of evolving species: every birth event comes with a corresponding chance for mutation, under the right condition and environment, said exotic entity might mature to reproduction, passing down its mutation. Natural selection is thus mirrored without any explicit logic. 

On an ecosystem scale, this philosophy plays into the parallelism strength of the GPU. Whilst the CPU generates the intial data, it is the shaders that feed on them to derive minimal rulesets and uniform algorithms to run simulation across countless active entities. Stateful, expensive events requiring readbacks to CPU are nonetheless present, though largely constrained to a tolerable frequency.

Every design decision prioritizes optimization, in form of sequential data access, cache-friendly logic, minimal atomic operations, and so on.

The procedural philosophy extends to most non-boilerplate systems, notably including terrain generation. No external assets are imported, meshes and data are fully code-generated, at least until serialization becomes necessary.



## Architecture

The engine runs two threads: a renderer (frame submission) and an async uploader, with intention to adopt a third thread dedicated to running CPU-side simulation, lifting off generation workload from the worker thread. 

Animals and plants primary buffers are fully preallocated during init, to be recycled in a queue-like pattern: deceased agents free up their own slot for newer generations. Consequently, the program gets to avoid expensive allocation during runtime. The large memory footprint does have its own constraint to prevent VRAM exhaustion and stress testing the upload flow. 

The project embraces the snappy, pixelated aesthetic, achieved via downsampling the presented framebuffer. Apart from a more distinctive visual identity, the technical implication is significantly reduced fragment's workload, enabling more liberty in (fragment-side) visual expression without noticably compromising performance. Along side such is general visual discrepancy's tolerance which enables slower simulation tick-rate (less frequent compute dispatches) and minor wait hiccups during high upload traffic.

The main camera is orthographic, isometric-like with only yaw rotation supported, further increasing optimization opportunities.



## Why Vulkan?

We can look at what Vulkan offers in the perspective of a dev of other standardized engines. Unity will be our point of comparison, its underlying architecture does adopt/mirror Vulkan among other graphics APIs, and I do have the most experience with it.

The project in its current state is undeniably recreatable in Unity. However, a large portion of their abstraction would see little-to-no use, oftentimes requiring workarounds throughout, all the while costing redundant setup time. Most importantly, the ecosystem likely will need to be scaled down to compromise for the engine's inherent overhead and architectural misalignment.

Unity does offer a middleground, between boilerplate graphics API code and abstraction black-box, being ScriptableRendererFeature (for URP, or full custom SRP if we take one step lower). One benefit of such is the capability to inject dispatches/calls at specific points in a frame, enabling more fine tuned optimization.  

At present, this Vulkan project is sidelined in pursuit of a Unity gamedev role due to time constraint. The benefit of having adopted Vulkan/OpenGL is a far more well-paced Unity learning process. Right from the start, I was more than willing to adopt advanced techniques, and every new knowledge went through a more nuanced lens, little was taken for granted. As a proof of concept, one can take a look at my first 2 Unity projects: 

At this point last year, if I had the choice between richer practical Unity experience versus foundational graphics APIs understanding, it would once again be the latter to help turn me into a better Unity developer in the long run.



## Code in this repository

This repository contains a representative selection of the codebase:

- `shaders/`: plant compute lifecycle, plant rendering, post-processing, heightmap generation.
- `src/riplants/`: L-system plant generation, DNA mutation and inheritance.

The engine layer is not included here, it is the largest part of the project by volume and the least readable without full context.


## Systems

| System | Status | Key Features |
|---|---|---|
| **Engine** | Functional | Setup boilerplate, bindless descriptor set, buffer allocation, pipeline abstraction, SDL2 windowing |
| **Worker Thread** | Functional | Dynamic rendering, isometric camera, Imgui, SDL2 input handling |
| **Upload Thread** | Drafted | Upload queueing, semaphore waiting |
| **Simulation Thread** | Planned | DNA/indirect generation logic, event signals handling |
| **Terrain** | Functional | Perlin/ridged heightmap generation, chunked instancing, per-texel fertility |
| **Plants DNA** | Drafted | Mutation bias, sustain cost curve |
| **Plants Rendering** | Functional | Growth scaling, senesence degradation |
| **Plants L-system** | Functional | L-system rule generation, L-system parser, turtle interpreter |
| **Plants Leafmask** | Functional | SDF silhouettes, per-leaf positioning, volumetric normals |
| **Animals DNA** | Planned | Mutation bias, sustain cost curve |
| **Animals Rendering** | Planned | *To be implemented* |
| **Animals Locomotion** | Drafted | Spine chain, IK approximation, appendage animation |
| **Animals Pathfinding** | Drafted | Cone probing, dynamic/static avoidance map |
| **Animals Bodymask** | Planned | *To be implemented* |
| **Water** | Drafted | Vertex-displaced wave, chunked instancing |
| **Clouds** | Drafted | Worley/perlin volumetric rendering, height-sliced ray marching |
| **Weathers** | Planned | *To be implemented* |
| **Post-processing** | Functional | Ping-pong chain, multi-pass bloom, rim-outline, volumetric god rays |


## Tech stack

- **Language:** C++20
- **Graphics API:** Vulkan 1.3 (vk-bootstrap, VMA)
- **Shaders:** GLSL compiled to SPIR-V via glslc
- **Windowing:** SDL2
- **Debug UI:** ImGui
- **Build:** CMake, Windows only