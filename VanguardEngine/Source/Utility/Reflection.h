// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <type_traits>

// Simple macro system for checking if a type contains a specific member.
// To use this, call VGMakeMemberCheck() with the member name of interest,
// called outside of the function you want to check members in. Then to perform
// the check, call VGHasMember() with the target type and member name.
// Credit: https://stackoverflow.com/a/16000226

#define VGDetailMakeMemberCheckBase(member) \
    template <typename T, typename = int> \
    struct VGHasMember_##member : std::false_type {};

#define VGDetailMakeMemberCheckBaseSpecialized(member) \
    template <typename T> \
    struct VGHasMember_##member<T, decltype((void)T::member, 0)> : std::true_type {};

#define VGMakeMemberCheck(member) \
    VGDetailMakeMemberCheckBase(member) \
    VGDetailMakeMemberCheckBaseSpecialized(member)

#define VGHasMember(T, member) \
    VGHasMember_##member<T>::value