#pragma once

#define kpanic(msg) kpanic_at(__FILE__, __LINE__, (msg))

__attribute__((noreturn)) void kpanic_at(const char *file, int line, const char *msg);
