#!/usr/bin/sh
INKSCAPE=inkscape
BASENAME=gp-icon

mkdir -p res/mipmap-mdpi res/mipmap-hdpi res/mipmap-xhdpi res/mipmap-xxhdpi res/mipmap-xxxhdpi

"$INKSCAPE" "$BASENAME.svg" -o "res/mipmap-mdpi/$BASENAME.png" --export-type=png --export-area-page --export-width=48 --export-height=48 --export-background='#000000' --export-background-opacity=0.0
"$INKSCAPE" "$BASENAME.svg" -o "res/mipmap-hdpi/$BASENAME.png" --export-type=png --export-area-page --export-width=72 --export-height=72 --export-background='#000000' --export-background-opacity=0.0
"$INKSCAPE" "$BASENAME.svg" -o "res/mipmap-xhdpi/$BASENAME.png" --export-type=png --export-area-page --export-width=96 --export-height=96 --export-background='#000000' --export-background-opacity=0.0
"$INKSCAPE" "$BASENAME.svg" -o "res/mipmap-xxhdpi/$BASENAME.png" --export-type=png --export-area-page --export-width=144 --export-height=144 --export-background='#000000' --export-background-opacity=0.0
"$INKSCAPE" "$BASENAME.svg" -o "res/mipmap-xxxhdpi/$BASENAME.png" --export-type=png --export-area-page --export-width=192 --export-height=192 --export-background='#000000' --export-background-opacity=0.0
