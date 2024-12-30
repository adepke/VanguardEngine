# Vanguard Rendering Engine
A Direct3D 12 real-time rendering sandbox for exploring evolving render tech.

## Key features:
- Physically-based shading
- Render graph handling all resources, barriers, and scheduling
- [Indirect GPU-driven rendering, hi-z occlusion and frustum mesh culling performed in compute shaders](https://youtu.be/WQRD-Eds0CU)
- Fully bindless resources (shader model 6.6+)
- [Precomputed atmospheric scattering model](https://youtu.be/rnKr92Yjrcc)
- [Clustered forward light rendering](https://youtu.be/Jj8EGCZFbLI)
- [Image-based lighting](https://youtu.be/QX5aG11s71w)
- [Bloom](https://youtu.be/UKhNTCVwqV4)
- Simple editor

![](https://user-images.githubusercontent.com/18013792/167507085-dbd68372-c2f5-414c-93f3-7391503a22d0.png)
![](https://user-images.githubusercontent.com/18013792/150621644-213dfcb8-2dbc-4841-ae60-f68f263fb39a.png)

## Building and running:
After cloning the repository, the included premake5 executable can be used to generate a solution file. The following command will produce a Visual Studio 2022 solution:

> $ **.\premake5.exe vs2022**

This `.sln` file can be opened in VS, and the Build Solution button can be used. MSBuild can be used manually if desired instead.

After getting the project compiled, you'll need the sample asset(s) to start the editor. For now, asset paths are hardcoded in `Engine.cpp`. To get the assets, [unzip this file](https://github.com/Xenonic/VanguardEngine/files/8886894/Models.zip) into `VanguardEngine/Assets/`.

From here, you can start the executable. Below are some errors you might run into and their solution:

> **Error:** Using the D3D12 SDKLayers dll requires that the latest SDKLayers for Windows 10 is installed \
> **Solution:** download the `Graphics Tools` additional feature for Windows 11. This can be found in the `Optional Features` section of the Windows Settings app.

> **Error:** Failed to load asset '*asset-name*.glb' \
> **Solution:** unzip the models zip file from the link above into the correct path and run again.
