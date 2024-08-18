import pygame
from pygame.locals import *

pygame.init()
screen = pygame.display.set_mode((16, 16))

with open("out.hex", "rb") as f:
    data = f.read()

surf = pygame.Surface((16, 16))

pixarr = pygame.pixelarray.PixelArray(surf)

for y in range(16):
    for x in range(16):
        pix_idx = y * 16 + x
        fb = pix_idx * 4
        bs = data[fb:fb+4]
        pixarr[x, y] = (int(bs[0]), int(bs[1]), int(bs[2]), int(bs[3]))

pygame.image.save(surf.convert_alpha(), "pimg.png")