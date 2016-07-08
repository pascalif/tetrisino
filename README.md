# tetrisino

Source code for a one-player Tetris game for Arduino Nano

## Features
+ Left and right rotations
+ Score management
+ Level and speed management
+ Game mode for next tetromino display
+ Game mode for automatic malus lines
+ 3 screensavers

## Hardware
Almost everything is configurable with a little bit of code tweaking :
+ Arduino Nano or similar
+ Display score and level on 8 7-digits
+ 6 8x8 pixel matrix
+ One joystick or any 4 actions input system
+ 3 buttons (rotations, start game)
+ 2 switchs
+ Piezo (music)

## MAME Arcade
See this code in action when implemented with a custom home-made MAME arcade station !
All the construction details and final results are here : https://lobrico.wordpress.com/2016/05/29/tetris/

## Tetris game
More informations (standards, rules, ...) on Tetris game :
+ http://colinfahey.com/tetris/tetris.html
+ http://tetris.wikia.com/wiki/Tetris_Guideline

## TODO list
+ Animation when new high score (in gotoGameOver())
+ Save high scores in EEPROM

## Credits
+ Base code : Jae Yeong Bae - jocker.tistory.com
+ Original music encoding : Luke Cyca - https://github.com/lukecyca/TetrisThemeArduino/blob/master/TetrisThemeArduino.ino
+ Refactoring, optimizations, above listed features : Pascal Vergneau
