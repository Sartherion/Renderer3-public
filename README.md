# Renderer3
A toy real-time deferred renderer using Direct3D 12 and written in C++. The purpose of the project is learning about modern and classic rendering techniques. The renderer is fully bindless and all data is passed to shaders via 32-bit root constants representing either an index into the shader-visible descriptor heap or an offset into a global read-only buffer resource. The project takes advantage of more recent additions to Direct3D 12, such as enhanced barriers, inline ray tracing, and GPU upload heaps.

## Features

### Ray-Traced Indirect Diffuse Lighting 
- per pixel indirect diffuse lighting with denoising based on self stabilizing recurrent blurs [[1, 2](#references)]
     - Blur passes use a poisson disk kernel, which is projected back onto the scene for spatially-aware blurring.
     - The blur radius is larger for newly-visible regions of the image and smaller for regions which have already temporally accumulated samples.
     - The blurred result from the previous frames is fed back into the current frame.
     - Disoccluded regions receive special treatment in a history fix pass, employing a dynamically-generated mip chain.
     - additionally provides denoised ray-traced ambient occlusion at no extra cost
       
     noisy input | denoised output | final image
     :---:|:---:|:---:
     ![](https://github.com/Sartherion/Renderer3-Images/blob/main/noisy2.png) | ![](https://github.com/Sartherion/Renderer3-Images/blob/main/denoised2_camera_movement.png) | ![](https://github.com/Sartherion/Renderer3-Images/blob/main/original2_camera_movement.png)
   *indirect diffuse lighting sampled with 1 ray per pixel* | *Note that the camera is moving in the snapshot, leading to imperfect denoising in disoccluded areas.*| *indirect diffuse buffer integrated into the final shading of the view*

- second-and-higher-bounce diffuse global illumination provided by ray-traced DDGI irradiance probes [[3, 4, 5](#references)]
     - infinite bounces by sampling the (previous) DDGI grid when shading hit points of probe update rays
     - light leaking reduced by maintaining per-probe depth and depth^2 maps, using a Chebyshev test when sampling probes
     - In addition to the irradiance atlas used for diffuse lighting, an additional radiance atlas generated from the same rays is stored, which can be used as a fallback for rough indirect specular lighting.
     - all atlases stored in the octahedral representation
  
  DDGI indirect diffuse | per-pixel indirect diffuse | path-traced reference
  :---:|:---:|:---:
  ![](https://github.com/Sartherion/Renderer3-Images/blob/main/ddgi.png) | ![](https://github.com/Sartherion/Renderer3-Images/blob/main/ddgi_per_pixel_comparison.png) | ![](https://github.com/Sartherion/Renderer3-Images/blob/main/ddgi_pt_reference.png)
  | *indirect diffuse lighting based solely on DDGI irradiance probes (shown as spheres); excerpts from depth, radiance, and irradiance atlases displayed in top left corner* | *first bounce per-pixel ray traced, subsequent bounces DDGI based* |

[![](https://img.youtube.com/vi/ZxIzYLs71RM/0.jpg)](https://www.youtube.com/watch?v=ZxIzYLs71RM)|
:---:|
*short video demonstration of the combined diffuse global-illumination solution*|

### Stochastic Screen-Space Reflections (SSSR) [[6](#references)]
- screen-space tracing done in a hierarchical fashion [[7](#references)], relying on a mip chain generated from the current depth buffer (depth pyramide)
- spatial and temporal reuse of samples, which are obtained via importance sampling of visible normals [[10](#references)]

real-time SSSR |  path-traced reference
:---:|:---:
![](https://github.com/Sartherion/Renderer3-Images/blob/main/sssr.png) | ![](https://github.com/Sartherion/Renderer3-Images/blob/main/sssr_pt_reference.png)

### Clustered Deferred / Forward Rendering
- main rendering performed in a clustered deferred fashion
- 128 bit G-buffer layout: 
  - R8G8B8A8_UNORM_SRGB: RGB = albedo, A = index into specular cube maps array
  - R16G16B16A16_UNORM: RG = octahedrally-encoded normals, B = metalness, A = roughness
  - R16G16_FLOAT: RG = motion vectors for temporal reprojection
- Clustering allows to have thousands of dynamic lights in the scene, as long as not too many lights overlap in the same cluster cell.
- Clustering is performed in mulitple passes on the GPU, following [[8](#references)].

     original scene | cluster debug visualization 
     :---:|:---:
     ![](https://github.com/Sartherion/Renderer3-Images/blob/main/clustered_shading.png) | ![](https://github.com/Sartherion/Renderer3-Images/blob/main/cluster_visualization.png)
  *scene with 2048 active unshadowed point lights* | *The view frustum gets divided into cells (8x8 pixel tiles and 64 depth slices), each of which gets its own linked list of lights employed for shading.*


  |[![](https://img.youtube.com/vi/TMuIy3aMWxQ/0.jpg)](https://www.youtube.com/watch?v=TMuIy3aMWxQ) 
  |:---:
  |*video demonstrating many dynamic unshadowed point lights* 
  
- Clustered forward rendering is also supported.
  | [![](https://img.youtube.com/vi/2nehqwLPui4/0.jpg)](https://www.youtube.com/watch?v=2nehqwLPui4)
  |:---:
  | *video portraying clustered forward rendering with many lights captured in a dynamic specular cube map*
  
### Shadowed Directional and Point Lights
- cascaded shadow maps for directional lights
     directional light cascaded shadow map | path-traced reference
     :-------------------------:|:-------------------------:
     ![](https://github.com/Sartherion/Renderer3-Images/blob/main/cascaded_shadowmap.png)*The debug view in the lower left corner displays the shadow cascades.*  |  ![](https://github.com/Sartherion/Renderer3-Images/blob/main/cascaded_shadowmap_pt_reference.png)

- point-light shadows via omnidirectional shadow maps
  |[![](https://img.youtube.com/vi/IRFDczwtoMc/0.jpg)](https://www.youtube.com/watch?v=IRFDczwtoMc)
  |:---:
  *video exhibiting 25 dynamic point lights with omnidirectional shadow maps*
  
### Visibility-Bitmask Screen-Space Ambient Occlusion [[9](#references)] 
- takes depth samples along randomly-chosen slices through the hemisphere around the surface normal
-  similar to horizon-based AO, but instead of yielding horizon angles, determines the visibility of equally-spaced sectors in the slice
- sector visibility stored in a 32-bit bitmask
- assumes a finite thickness for depth buffer, allowing light to pass behind objects
  
bilaterally-blurred screen-space AO |  combined AO    | denoised ray-traced AO
:---:|:---:|:---:
![](https://github.com/Sartherion/Renderer3-Images/blob/main/ss_ao.png) | ![](https://github.com/Sartherion/Renderer3-Images/blob/main/rt_ss_ao.png) | ![](https://github.com/Sartherion/Renderer3-Images/blob/main/rt_ao.png)

### Temporal Anti-Aliasing
- reduces aliasing by reusing reprojected samples from previous frames
- provides extra denoising for stochastic per-pixel rendering techniques

### Interactive Reference Path-Tracing Mode 
- When implementing real-time global illumination effects, it is often very useful to have a close-to-ground-truth reference image for the current view and lighting conditions.
     real-time rendered scene |  path-traced reference
     :---:|:---:
     ![](https://github.com/Sartherion/Renderer3-Images/blob/main/one_light2_indirect_non-sh.png) | ![](https://github.com/Sartherion/Renderer3-Images/blob/main/one_light2_pt_reference.png)

## References

[**1**] D. Zhdan, "ReBLUR: A Hierarchical Recurrent Denoiser", *Ray Tracing Gems II*, 2021.

[**2**] D. Zhdan, "Fast denoising with self stabilizing recurrent blurs", *Game Developers Conference*, 2020.

[**3**] M. McGuire, "Ray-Traced Irradiance Fields", *Game Developers Conference*, 2019.

[**4**] Z. Majercik, J.-P. Guertin, D. Nowrouzezahrai, and M. McGuire, "Dynamic Diffuse Global Illumination with Ray-Traced Irradiance Fields", *Journal of Computer Graphics Techniques*, 2019.

[**5**] Z. Majercik, A. Marrs, J. Spjut, and M. McGuire, "Scaling Probe-Based Real-Time Dynamic Global Illumination for Production", *Journal of Computer Graphics Techniques*, 2021.

[**6**] T. Stachowiak and Y. Uludag, "Stochastic Screen-Space Reflections", *SIGGRAPH Advances in Real-time Rendering course*, 2015. 

[**7**] Y. Uludag, "Hi-Z Screen-Space Cone-Traced Reﬂections", *GPU Pro 5*, 2014. 

[**8**] K. Ortegren and E. Persson, "Clustered Shading: Assigning Lights Using Conservative Rasterization in DirectX 12", *GPU Pro 7*, 2016.

[**9**] O. Therrien, Y. Levesque, and G. Gilet, "Screen Space Indirect Lighting with Visibility Bitmask", *arXiv:2301.11376*, 2023.

[**10**] E. Heitz, "Sampling the GGX Distribution of Visible Normals", *Journal of Computer Graphics Techniques*, 2018.

## External Libraries

- [DearImgui](https://github.com/ocornut/imgui)
- [OffsetAllocator](https://github.com/sebbbi/OffsetAllocator)
- [rapidobj](https://github.com/guybrush77/rapidobj)
