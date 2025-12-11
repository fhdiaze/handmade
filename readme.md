# Handmade hero

## Arch
Virtualising the game for the OS (platform)

- "->" means use

os -> platform <-> core

## Unity build
- "->" means include

plat_win_handmade.c -> game.c <- plat_lin_handmade.c
                         |
                         v
                       game.h