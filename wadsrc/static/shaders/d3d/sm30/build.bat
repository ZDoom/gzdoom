fxc ..\shaders.ps /Tps_3_0 /O3 /ENormalColor -DPALTEX=0 -DINVERT=0 -DOPAQUE=0 -DSTENCIL=0 -DALPHATEX=0 -DDESAT=0 /FoNormalColor.pso
fxc ..\shaders.ps /Tps_3_0 /O3 /ENormalColor -DPALTEX=1 -DINVERT=0 -DOPAQUE=0 -DSTENCIL=0 -DALPHATEX=0 -DDESAT=0 /FoNormalColorPal.pso

fxc ..\shaders.ps /Tps_3_0 /O3 /ENormalColor -DPALTEX=0 -DINVERT=1 -DOPAQUE=0 -DSTENCIL=0 -DALPHATEX=0 -DDESAT=1 /FoNormalColorInv.pso
fxc ..\shaders.ps /Tps_3_0 /O3 /ENormalColor -DPALTEX=1 -DINVERT=1 -DOPAQUE=0 -DSTENCIL=0 -DALPHATEX=0 -DDESAT=1 /FoNormalColorPalInv.pso

fxc ..\shaders.ps /Tps_3_0 /O3 /ENormalColor -DPALTEX=0 -DINVERT=0 -DOPAQUE=1 -DSTENCIL=0 -DALPHATEX=0 -DDESAT=1 /FoNormalColorOpaq.pso
fxc ..\shaders.ps /Tps_3_0 /O3 /ENormalColor -DPALTEX=1 -DINVERT=0 -DOPAQUE=1 -DSTENCIL=0 -DALPHATEX=0 -DDESAT=1 /FoNormalColorPalOpaq.pso

fxc ..\shaders.ps /Tps_3_0 /O3 /ENormalColor -DPALTEX=0 -DINVERT=1 -DOPAQUE=1 -DSTENCIL=0 -DALPHATEX=0 -DDESAT=1 /FoNormalColorInvOpaq.pso
fxc ..\shaders.ps /Tps_3_0 /O3 /ENormalColor -DPALTEX=1 -DINVERT=1 -DOPAQUE=1 -DSTENCIL=0 -DALPHATEX=0 -DDESAT=1 /FoNormalColorPalInvOpaq.pso

fxc ..\shaders.ps /Tps_3_0 /O3 /ENormalColor -DPALTEX=0 -DINVERT=0 -DOPAQUE=0 -DSTENCIL=1 -DALPHATEX=0 -DDESAT=0 /FoStencil.pso
fxc ..\shaders.ps /Tps_3_0 /O3 /ENormalColor -DPALTEX=1 -DINVERT=0 -DOPAQUE=0 -DSTENCIL=1 -DALPHATEX=0 -DDESAT=0 /FoPalStencil.pso

fxc ..\shaders.ps /Tps_3_0 /O3 /ENormalColor -DPALTEX=0 -DINVERT=0 -DOPAQUE=0 -DSTENCIL=0 -DALPHATEX=1 -DDESAT=0 /FoAlphaTex.pso
fxc ..\shaders.ps /Tps_3_0 /O3 /ENormalColor -DPALTEX=1 -DINVERT=0 -DOPAQUE=0 -DSTENCIL=0 -DALPHATEX=1 -DDESAT=0 /FoPalAlphaTex.pso

fxc ..\shaders.ps /Tps_3_0 /O3 /ENormalColor -DPALTEX=0 -DINVERT=0 -DOPAQUE=0 -DSTENCIL=0 -DALPHATEX=0 -DDESAT=1 /FoNormalColorD.pso
fxc ..\shaders.ps /Tps_3_0 /O3 /ENormalColor -DPALTEX=1 -DINVERT=0 -DOPAQUE=0 -DSTENCIL=0 -DALPHATEX=0 -DDESAT=1 /FoNormalColorPalD.pso

fxc ..\shaders.ps /Tps_3_0 /O3 /EVertexColor /FoVertexColor.pso

fxc ..\shaders.ps /Tps_3_0 /O3 /ESpecialColormap -DPALTEX=0 -DINVERT=0 -DOPAQUE=0 -DSTENCIL=0 -DALPHATEX=0 -DDESAT=0  /FoSpecialColormap.pso
fxc ..\shaders.ps /Tps_3_0 /O3 /ESpecialColormap -DPALTEX=1 -DINVERT=0 -DOPAQUE=0 -DSTENCIL=0 -DALPHATEX=0 -DDESAT=0  /FoSpecialColormapPal.pso

fxc ..\shaders.ps /Tps_3_0 /O3 /EBurnWipe /FoBurnWipe.pso

fxc ..\shaders.ps /Tps_3_0 /O3 /EGammaCorrection /FoGammaCorrection.pso
