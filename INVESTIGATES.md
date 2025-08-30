En cas de problème avec le Tetris

Compilation
===========
Attention à la mémoire restante !

"
Sketch uses 22204 bytes (72%) of program storage space. Maximum is 30720 bytes.
Global variables use 1435 bytes (70%) of dynamic memory, leaving 613 bytes for local variables. Maximum is 2048 bytes.
"

Toute chaine de caractère consomment des "Global variables".

Au maximum des options activées (DEV_input + DEV_log + ENABLE_MUSIC), en 2025-08:
	Sketch uses 24518 bytes (79%) of program storage space. Maximum is 30720 bytes.
	Global variables use 1885 bytes (92%) of dynamic memory, leaving 163 bytes for local variables. Maximum is 2048 bytes.
	=> CA PLANTE (dès le boot)

Avec juste ENABLE_MUSIC
	Sketch uses 23802 bytes (77%) of program storage space. Maximum is 30720 bytes.
	Global variables use 1723 bytes (84%) of dynamic memory, leaving 325 bytes for local variables. Maximum is 2048 bytes.
	=> CA FONCTIONNE

Avec ADebouncers:
	Sketch uses 24306 bytes (79%) of program storage space. Maximum is 30720 bytes.
	Global variables use 1765 bytes (86%) of dynamic memory, leaving 283 bytes for local variables. Maximum is 2048 bytes.
	=> CA FONCTIONNE


Instabilités
============
Si s'éteint/s'allume (en particulier bip on puis bip off du PC quand la facade arrière est connectée à l'usb du PC)
Alors vérifier les résistances des cables VCC et GND avant/apres le master switch
  Si 80 Ohm de différence entre le VCC 5V de l'usb facade et la pin en sortie du switch : pas assez pour alimenter le Nano.
  Alors, vérifier les connections
  	- dans les dominos blancs,
  	- dans les serrages des vis sur le switch pour réduire la résistance.
  	- dans le choix des pins sur le mini breadboard rouge
  Résistance minimale vue sur toute la chaine (i.e. possible) = 5.8 Ohm (sur GND)

  Si pas de solution, le bypasser temporairement sur le mini breadboard rouge


Autres pbs
==========
- si les MAX7219 ne réagissent pas, ils sont peut être mal pluggés sur le bon VCC 5V