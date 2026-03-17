inline void *operator new(size_t size, MemoryFlagType flag) {
	return (Alloc(size, flag));
}
inline void *operator new[](size_t size, MemoryFlagType flag) {
	return (Alloc(size, flag));
}

inline void *Add_Long_To_Pointer(void const *ptr, long size) {
	return ((void *)((char const *)ptr + size));
}
