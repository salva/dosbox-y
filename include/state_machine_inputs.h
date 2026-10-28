/* Immutable machine inputs are contributed by the owners that resolve/load
 * them, not reopened by archive capture. Mutable DOS files/media are excluded. */
#ifndef DOSBOX_STATE_MACHINE_INPUTS_H
#define DOSBOX_STATE_MACHINE_INPUTS_H
#include <stddef.h>
#include <string>
#include <stdio.h>
void STATE_RecordMachinePath(const std::string &owner, const std::string &path);
void STATE_RecordMachineFile(const std::string &owner, FILE *file);
void STATE_RecordMachineInput(const std::string &owner, const void *bytes, size_t size);
void STATE_RetireMachineInput(const std::string &owner);
void STATE_MachineInputsUnavailable();
std::string STATE_MachineInputsIdentity();
#endif
