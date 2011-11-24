#include <unistd.h>
#include <uniso.h>

int main(int argc, char *argv[])
{
	return uniso(STDIN_FILENO, NULL, NULL);
}
