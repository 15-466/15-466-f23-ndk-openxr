#!/usr/bin/sh
INKSCAPE=inkscape
BASENAME=gp-icon

mkdir -p mipmap-mdpi mipmap-hdpi mipmap-xhdpi mipmap-xxhdpi mipmap-xxxhdpi

"$INKSCAPE" "$BASENAME.svg" -o "mipmap-mdpi/$BASENAME.png" --export-type=png --export-area-page --export-width=48 --export-height=48 --export-background='#000000' --export-background-opacity=0.0
"$INKSCAPE" "$BASENAME.svg" -o "mipmap-hdpi/$BASENAME.png" --export-type=png --export-area-page --export-width=72 --export-height=72 --export-background='#000000' --export-background-opacity=0.0
"$INKSCAPE" "$BASENAME.svg" -o "mipmap-xhdpi/$BASENAME.png" --export-type=png --export-area-page --export-width=96 --export-height=96 --export-background='#000000' --export-background-opacity=0.0
"$INKSCAPE" "$BASENAME.svg" -o "mipmap-xxhdpi/$BASENAME.png" --export-type=png --export-area-page --export-width=144 --export-height=144 --export-background='#000000' --export-background-opacity=0.0
"$INKSCAPE" "$BASENAME.svg" -o "mipmap-xxxhdpi/$BASENAME.png" --export-type=png --export-area-page --export-width=192 --export-height=192 --export-background='#000000' --export-background-opacity=0.0
