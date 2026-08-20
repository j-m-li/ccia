
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

int main(int argc, char *argv[])
{
	stbi_uc *u;
	int x, y, c;
	u = stbi_load("tests/z06n2c08.png", &x,&y, &c, 4);
	if (!u) return -1;
	return 0;
}

