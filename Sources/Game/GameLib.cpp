extern void __RegisterEnum__td__CarDrawableComponent__CarKind();
extern void __RegisterEnum__td__GroundKind();
extern void __RegisterEnum__td__SessionState();
extern void __RegisterEnum__td__GameTutorialComponent__Step();
extern void __RegisterEnum__td__Dir();
extern void __RegisterClass__td__CarDrawableComponent();
extern void __RegisterClass__td__GameControllerComponent();
extern void __RegisterClass__td__GameHUDComponent();
extern void __RegisterClass__td__GameTutorialComponent();
extern void __RegisterClass__td__TiltShiftPass();
extern void __RegisterClass__td__TokenVfxComponent();
extern void __RegisterClass__td__TrafficCarComponent();


extern void InitializeTypesGameLib()
{
    __RegisterEnum__td__CarDrawableComponent__CarKind();
    __RegisterEnum__td__GroundKind();
    __RegisterEnum__td__SessionState();
    __RegisterEnum__td__GameTutorialComponent__Step();
    __RegisterEnum__td__Dir();
    __RegisterClass__td__CarDrawableComponent();
    __RegisterClass__td__GameControllerComponent();
    __RegisterClass__td__GameHUDComponent();
    __RegisterClass__td__GameTutorialComponent();
    __RegisterClass__td__TiltShiftPass();
    __RegisterClass__td__TokenVfxComponent();
    __RegisterClass__td__TrafficCarComponent();
}