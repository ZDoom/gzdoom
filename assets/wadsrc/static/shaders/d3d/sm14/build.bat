fxc ..\shaders.ps /Tps_1_4 /LD -DPS14=1 /ENormalColor -DPALTEX=0 -DINVERT=0 /FoNormalColor.pso
fxc ..\shaders.ps /Tps_1_4 /LD -DPS14=1 /ENormalColor -DPALTEX=0 -DINVERT=1 /FoNormalColorInv.pso
fxc ..\shaders.ps /Tps_1_4 /LD -DPS14=1 /ENormalColor -DPALTEX=1 -DINVERT=0 /FoNormalColorPal.pso
fxc ..\shaders.ps /Tps_1_4 /LD -DPS14=1 /ENormalColor -DPALTEX=1 -DINVERT=1 /FoNormalColorPalInv.pso

fxc ..\shaders.ps /Tps_1_4 /LD -DPS14=1 /ERedToAlpha -DINVERT=0 /FoRedToAlpha.pso
fxc ..\shaders.ps /Tps_1_4 /LD -DPS14=1 /ERedToAlpha -DINVERT=1 /FoRedToAlphaInv.pso

fxc ..\shaders.ps /Tps_1_4 /LD -DPS14=1 /EVertexColor /FoVertexColor.pso

fxc ..\shaders.ps /Tps_1_4 /LD -DPS14=1 /ESpecialColormap -DPALTEX=0 -DINVERT=0 /FoSpecialColormap.pso
fxc ..\shaders.ps /Tps_1_4 /LD -DPS14=1 /ESpecialColormap -DPALTEX=1 -DINVERT=0 /FoSpecialColormapPal.pso

fxc ..\shaders.ps /Tps_1_4 /LD -DPS14=1 /EInGameColormap -DPALTEX=0 -DINVERT=0 -DDESAT=0 /FoInGameColormap.pso
fxc ..\shaders.ps /Tps_1_4 /LD -DPS14=1 /EInGameColormap -DPALTEX=0 -DINVERT=0 -DDESAT=1 /FoInGameColormapDesat.pso
fxc ..\shaders.ps /Tps_1_4 /LD -DPS14=1 /EInGameColormap -DPALTEX=0 -DINVERT=1 -DDESAT=0 /FoInGameColormapInv.pso
fxc ..\shaders.ps /Tps_1_4 /LD -DPS14=1 /EInGameColormap -DPALTEX=0 -DINVERT=1 -DDESAT=1 /FoInGameColormapInvDesat.pso
fxc ..\shaders.ps /Tps_1_4 /LD -DPS14=1 /EInGameColormap -DPALTEX=1 -DINVERT=0 -DDESAT=0 /FoInGameColormapPal.pso
fxc ..\shaders.ps /Tps_1_4 /LD -DPS14=1 /EInGameColormap -DPALTEX=1 -DINVERT=0 -DDESAT=1 /FoInGameColormapPalDesat.pso
fxc ..\shaders.ps /Tps_1_4 /LD -DPS14=1 /EInGameColormap -DPALTEX=1 -DINVERT=1 -DDESAT=0 /FoInGameColormapPalInv.pso
fxc ..\shaders.ps /Tps_1_4 /LD -DPS14=1 /EInGameColormap -DPALTEX=1 -DINVERT=1 -DDESAT=1 /FoInGameColormapPalInvDesat.pso

fxc ..\shaders.ps /Tps_1_4 /LD -DPS14=1 /EBurnWipe /FoBurnWipe.pso

fxc ..\shaders.ps /Tps_1_4 /LD -DPS14=1 /EGammaCorrection /FoGammaCorrection.pso
