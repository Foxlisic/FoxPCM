#include "av.h"

int main(int argc, char* argv[]) {

    C65* c65 = new C65(640, 400, 2, 25);

    c65->reset();
    c65->load(argc, argv);

    while (c65->main()) c65->frame();
    return 0;
}
