# Create base64 image data

Use chrome as an example.

On Windows, find chrome.exe, copy it to somewhere else, extract it, we will get a chrome folder.

Inside chrome, in .rsrc/ICON, find a small and good quality icon, such as 3.ico (size 48x48)

On Ubuntu for Windows:

    sudo apt install imagemagick

Convert ico to png:

    convert 3.ico -thumbnail 32x32 -alpha on -background none -flatten chrome.png

get base64 encoded image:

    python encode_b64img.py chrome.png
