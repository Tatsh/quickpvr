#include "module.h"

namespace QuickPvrShell {

namespace {

HINSTANCE module = nullptr;
std::atomic<long> moduleReferences{0};

} // namespace

HINSTANCE moduleHandle() {
    return module;
}

void setModuleHandle(HINSTANCE handle) {
    module = handle;
}

void addModuleReference() {
    ++moduleReferences;
}

void releaseModuleReference() {
    --moduleReferences;
}

bool hasModuleReferences() {
    return moduleReferences.load() != 0;
}

ComObject::ComObject() {
    addModuleReference();
}

ComObject::~ComObject() {
    releaseModuleReference();
}

ULONG ComObject::addReference() {
    return ++referenceCount_;
}

ULONG ComObject::release() {
    const auto count = --referenceCount_;
    if (count == 0) {
        delete this;
    }
    return count;
}

} // namespace QuickPvrShell
