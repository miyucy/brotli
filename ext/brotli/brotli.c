#include "brotli.h"

VALUE rb_mBrotli;

void
Init_brotli(void)
{
  rb_mBrotli = rb_define_module("Brotli");
}
