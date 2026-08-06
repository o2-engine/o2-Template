extern void __RegisterClass__DragonDefenseBootstrap();
extern void __RegisterClass__DragonInputComponent();
extern void __RegisterClass__PyramidSpawner();
extern void __RegisterClass__RotatorComponent();


extern void InitializeTypesGameLib()
{
    __RegisterClass__DragonDefenseBootstrap();
    __RegisterClass__DragonInputComponent();
    __RegisterClass__PyramidSpawner();
    __RegisterClass__RotatorComponent();
}