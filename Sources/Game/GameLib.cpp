extern void __RegisterEnum__CarDrawableComponent__CarKind();
extern void __RegisterEnum__td__GroundKind();
extern void __RegisterEnum__td__SessionState();
extern void __RegisterEnum__td__GameTutorial__Step();
extern void __RegisterEnum__td__Dir();
extern void __RegisterClass__PyramidSpawner();
extern void __RegisterClass__RotatorComponent();
extern void __RegisterClass__CarDrawableComponent();
extern void __RegisterClass__GameControllerComponent();
extern void __RegisterClass__TiltShiftPass();


extern void InitializeTypesGameLib()
{
    __RegisterEnum__CarDrawableComponent__CarKind();
    __RegisterEnum__td__GroundKind();
    __RegisterEnum__td__SessionState();
    __RegisterEnum__td__GameTutorial__Step();
    __RegisterEnum__td__Dir();
    __RegisterClass__PyramidSpawner();
    __RegisterClass__RotatorComponent();
    __RegisterClass__CarDrawableComponent();
    __RegisterClass__GameControllerComponent();
    __RegisterClass__TiltShiftPass();
}