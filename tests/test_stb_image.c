
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

int main(int argc, char *argv[])
{
	stbi_uc *u;
	int x, y, c;
	u = stbi_load("tests/z06n2c08.png", &x,&y, &c, 0);
	if (!u) return -1; 
	stbi_write_jpg("tests/stb.jpg", x, y, c, u, 100);	
	return 0;
}

