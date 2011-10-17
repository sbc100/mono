#include <glib.h>
#include <string.h>

#include "monobitset.h"
#include "config.h"

#define BITS_PER_CHUNK MONO_BITSET_BITS_PER_CHUNK

/*
 * mono_bitset_alloc_size:
 * @max_size: The number of bits you want to hold
 * @flags: unused
 *
 * Return the number of bytes required to hold the bitset.
 * Useful to allocate it on the stack or with mempool.
 * Use with mono_bitset_mem_new ().
 */
guint32
mono_bitset_alloc_size (guint32 max_size, guint32 flags) {
	guint32 real_size = (max_size + BITS_PER_CHUNK - 1) / BITS_PER_CHUNK;

	return sizeof (MonoBitSet) + sizeof (gsize) * (real_size - MONO_ZERO_LEN_ARRAY);
}

/*
 * mono_bitset_new:
 * @max_size: The numer of bits you want to hold
 * @flags: bitfield of flags
 *
 * Return a bitset of size max_size. It must be freed using
 * mono_bitset_free.
 */
MonoBitSet *
mono_bitset_new (guint32 max_size, guint32 flags) {
	guint32 real_size = (max_size + BITS_PER_CHUNK - 1) / BITS_PER_CHUNK;
	MonoBitSet *result;

	result = g_malloc0 (sizeof (MonoBitSet) + sizeof (gsize) * (real_size - MONO_ZERO_LEN_ARRAY));
	result->size = real_size * BITS_PER_CHUNK;
	result->flags = flags;
	return result;
}

/*
 * mono_bitset_mem_new:
 * @mem: The location the bitset is stored
 * @max_size: The number of bits you want to hold
 * @flags: bitfield of flags
 *
 * Return mem, which is now a initialized bitset of size max_size. It is
 * not freed even if called with mono_bitset_free. mem must be at least
 * as big as mono_bitset_alloc_size returns for the same max_size.
 */
MonoBitSet *
mono_bitset_mem_new (gpointer mem, guint32 max_size, guint32 flags) {
	guint32 real_size = (max_size + BITS_PER_CHUNK - 1) / BITS_PER_CHUNK;
	MonoBitSet *result = mem;

	result->size = real_size * BITS_PER_CHUNK;
	result->flags = flags | MONO_BITSET_DONT_FREE;
	return result;
}

/*
 * mono_bitset_free:
 * @set: bitset ptr to free
 *
 * Free bitset unless flags have MONO_BITSET_DONT_FREE set. Does not
 * free anything if flag MONO_BITSET_DONT_FREE is set or bitset was
 * made with mono_bitset_mem_new.
 */
void
mono_bitset_free (MonoBitSet *set) {
	if (!(set->flags & MONO_BITSET_DONT_FREE))
		g_free (set);
}

/*
 * mono_bitset_set:
 * @set: bitset ptr
 * @pos: set bit at this pos
 *
 * Set bit at pos @pos, counting from 0.
 */
void
mono_bitset_set (MonoBitSet *set, guint32 pos) {
	int j = pos / BITS_PER_CHUNK;
	int bit = pos % BITS_PER_CHUNK;

	g_assert (pos < set->size);

	set->data [j] |= (gsize)1 << bit;
}

/*
 * mono_bitset_test:
 * @set: bitset ptr
 * @pos: test bit at this pos
 *
 * Test bit at pos @pos, counting from 0.
 * Returns a value != 0 if set, 0 otherwise.
 */
int
mono_bitset_test (const MonoBitSet *set, guint32 pos) {
	int j = pos / BITS_PER_CHUNK;
	int bit = pos % BITS_PER_CHUNK;

	g_return_val_if_fail (pos < set->size, 0);

	return (set->data [j] & ((gsize)1 << bit)) > 0;
}

/*
 * mono_bitset_test_bulk:
 * @set: bitset ptr
 * @pos: test bit at this pos
 *
 * Return 32/64 bits from the bitset, starting from @pos, which must be 
 * divisible with 32/64.
 */
gsize
mono_bitset_test_bulk (const MonoBitSet *set, guint32 pos) {
	int j = pos / BITS_PER_CHUNK;

	if (pos >= set->size)
		return 0;
	else
		return set->data [j];
}

/*
 * mono_bitset_clear:
 * @set: bitset ptr
 * @pos: unset bit at this pos
 *
 * Unset bit at pos 'pos', counting from 0.
 */
void
mono_bitset_clear (MonoBitSet *set, guint32 pos) {
	int j = pos / BITS_PER_CHUNK;
	int bit = pos % BITS_PER_CHUNK;

	g_assert (pos < set->size);

	set->data [j] &= ~((gsize)1 << bit);
}

/*
 * mono_bitset_clear_all:
 * @set: bitset ptr
 *
 * Unset all bits.
 */
void
mono_bitset_clear_all (MonoBitSet *set) {
	memset (set->data, 0, set->size / 8);
}

/*
 * mono_bitset_set_all:
 * @set: bitset ptr
 *
 * Set all bits.
 */
void
mono_bitset_set_all (MonoBitSet *set) {
	memset (set->data, -1, set->size / 8);
}

/*
 * mono_bitset_invert:
 * @set: bitset ptr
 *
 * Flip all bits.
 */
void
mono_bitset_invert (MonoBitSet *set) {
	int i;
	for (i = 0; i < set->size / BITS_PER_CHUNK; ++i)
		set->data [i] = ~set->data [i];
}

/*
 * mono_bitset_size:
 * @set: bitset ptr
 *
 * Returns the number of bits this bitset can hold.
 */
guint32
mono_bitset_size (const MonoBitSet *set) {
	return set->size;
}

/* 
 * should test wich version is faster.
 */
#if 1

/*
 * mono_bitset_count:
 * @set: bitset ptr
 *
 * return number of bits that is set.
 */
guint32
mono_bitset_count (const MonoBitSet *set) {
	guint32 i, count;
	gsize d;

	count = 0;
	for (i = 0; i < set->size / BITS_PER_CHUNK; ++i) {
		d = set->data [i];
		/* there is probably some asm code that can do this much faster */
		if (d) {
#if SIZEOF_VOID_P == 8
			/* http://www.jjj.de/bitwizardry/bitwizardrypage.html */
			d -=  (d>>1) & 0x5555555555555555;
			d  = ((d>>2) & 0x3333333333333333) + (d & 0x3333333333333333);
			d  = ((d>>4) + d) & 0x0f0f0f0f0f0f0f0f;
			d *= 0x0101010101010101;
			count += d >> 56;
#else
			/* http://aggregate.org/MAGIC/ */
			d -= ((d >> 1) & 0x55555555);
			d = (((d >> 2) & 0x33333333) + (d & 0x33333333));
			d = (((d >> 4) + d) & 0x0f0f0f0f);
			d += (d >> 8);
			d += (d >> 16);
			count += (d & 0x0000003f);
#endif
		}
	}
	return count;
}
#else
guint32
mono_bitset_count (const MonoBitSet *set) {
	static const guint32 table [] = {
		0x55555555, 0x33333333, 0x0F0F0F0F, 0x00FF00FF, 0x0000FFFF
	};
	guint32 i, count, val;

	count = 0;
	for (i = 0; i < set->size / BITS_PER_CHUNK;+i) {
		if (set->data [i]) {
			val = set->data [i];
			val = (val & table [0]) ((val >> 1) & table [0]);
			val = (val & table [1]) ((val >> 2) & table [1]);
			val = (val & table [2]) ((val >> 4) & table [2]);
			val = (val & table [3]) ((val >> 8) & table [3]);
			val = (val & table [4]) ((val >> 16) & table [4]);
			count += val;
		}
	}
	return count;
}

#endif

#if 0
const static int 
bitstart_mask [] = {
	0xffffffff, 0xfffffffe, 0xfffffffc, 0xfffffff8,
	0xfffffff0, 0xffffffe0, 0xffffffc0, 0xffffff80,
	0xffffff00, 0xfffffe00, 0xfffffc00, 0xfffff800,
	0xfffff000, 0xffffe000, 0xffffc000, 0xffff8000,
	0xffff0000, 0xfffe0000, 0xfffc0000, 0xfff80000,
	0xfff00000, 0xffe00000, 0xffc00000, 0xff800000,
	0xff000000, 0xfe000000, 0xfc000000, 0xf8000000,
	0xf0000000, 0xe0000000, 0xc0000000, 0x80000000,
	0x00000000
};

#define my_g_bit_nth_lsf(m,n) (ffs((m) & bitstart_mask [(n)+1])-1)
#define my_g_bit_nth_lsf_nomask(m) (ffs((m))-1)

#else

static inline gint
my_g_bit_nth_lsf (gsize mask, gint nth_bit)
{
	nth_bit ++;
	mask >>= nth_bit;

	if ((mask == 0) || (nth_bit == BITS_PER_CHUNK))
		return -1;

#if (defined(__i386__) || defined (__native_client__)) && defined(__GNUC__)
 {
	 int r;
	 /* This depends on mask != 0 */
	 __asm__("bsfl %1,%0\n\t"
			 : "=r" (r) : "g" (mask)); 
	 return nth_bit + r;
 }
#elif defined(__x86_64) && defined(__GNUC__)
 {
	guint64 r;

	__asm__("bsfq %1,%0\n\t"
			: "=r" (r) : "rm" (mask));
	return nth_bit + r;
 }
#else
	while (! (mask & 0x1)) {
		mask >>= 1;
		nth_bit ++;
	}

	return nth_bit;
#endif
}

static inline gint
my_g_bit_nth_lsf_nomask (gsize mask)
{
	/* Mask is expected to be != 0 */
#if defined(__i386__) && defined(__GNUC__)
	int r;

	__asm__("bsfl %1,%0\n\t"
			: "=r" (r) : "rm" (mask));
	return r;
#elif defined(__x86_64) && defined(__GNUC__)
	guint64 r;

	__asm__("bsfq %1,%0\n\t"
			: "=r" (r) : "rm" (mask));
	return r;
#else
	int nth_bit = 0;

	while (! (mask & 0x1)) {
		mask >>= 1;
		nth_bit ++;
	}

	return nth_bit;
#endif
}

#endif

static inline int
my_g_bit_nth_msf (gsize mask,
	       gint   nth_bit)
{
	int i;

	if (nth_bit == 0)
		return -1;

	mask <<= BITS_PER_CHUNK - nth_bit;

	i = BITS_PER_CHUNK;
	while ((i > 0) && !(mask >> (BITS_PER_CHUNK - 8))) {
		mask <<= 8;
		i -= 8;
	}
	if (mask == 0)
		return -1;

	do {
		i--;
		if (mask & ((gsize)1 << (BITS_PER_CHUNK - 1)))
			return i - (BITS_PER_CHUNK - nth_bit);
		mask <<= 1;
    }
	while (mask);

	return -1;
}

static int
find_first_unset (gsize mask, gint nth_bit)
{
	do {
		nth_bit++;
		if (!(mask & ((gsize)1 << nth_bit))) {
			if (nth_bit == BITS_PER_CHUNK)
				/* On 64 bit platforms, 1 << 64 == 1 */
				return -1;
			else
				return nth_bit;
		}
	} while (nth_bit < BITS_PER_CHUNK);
	return -1;
}

/*
 * mono_bitset_find_start:
 * @set: bitset ptr
 *
 * Equivalent to mono_bitset_find_first (set, -1) but faster.
 */
int
mono_bitset_find_start   (const MonoBitSet *set)
{
	int i;

	for (i = 0; i < set->size / BITS_PER_CHUNK; ++i) {
		if (set->data [i])
			return my_g_bit_nth_lsf_nomask (set->data [i]) + i * BITS_PER_CHUNK;
	}
	return -1;
}

/*
 * mono_bitset_find_first:
 * @set: bitset ptr
 * @pos: pos to search _after_ (not including)
 *
 * Returns position of first set bit after @pos. If pos < 0 begin search from
 * start. Return -1 if no bit set is found.
 */
int
mono_bitset_find_first (const MonoBitSet *set, gint pos) {
	int j;
	int bit;
	int result, i;

	if (pos < 0) {
		j = 0;
		bit = -1;
	} else {
		j = pos / BITS_PER_CHUNK;
		bit = pos % BITS_PER_CHUNK;
		g_assert (pos < set->size);
	}
	/*g_print ("find first from %d (j: %d, bit: %d)\n", pos, j, bit);*/

	if (set->data [j]) {
		result = my_g_bit_nth_lsf (set->data [j], bit);
		if (result != -1)
			return result + j * BITS_PER_CHUNK;
	}
	for (i = ++j; i < set->size / BITS_PER_CHUNK; ++i) {
		if (set->data [i])
			return my_g_bit_nth_lsf (set->data [i], -1) + i * BITS_PER_CHUNK;
	}
	return -1;
}

/*
 * mono_bitset_find_last:
 * @set: bitset ptr
 * @pos: pos to search _before_ (not including)
 *
 * Returns position of last set bit before pos. If pos < 0 search is
 * started from the end. Returns -1 if no set bit is found.
 */
int
mono_bitset_find_last (const MonoBitSet *set, gint pos) {
	int j, bit, result, i;

	if (pos < 0)
		pos = set->size - 1;
		
	j = pos / BITS_PER_CHUNK;
	bit = pos % BITS_PER_CHUNK;

	g_return_val_if_fail (pos < set->size, -1);

	if (set->data [j]) {
		result = my_g_bit_nth_msf (set->data [j], bit);
		if (result != -1)
			return result + j * BITS_PER_CHUNK;
	}
	for (i = --j; i >= 0; --i) {
		if (set->data [i])
			return my_g_bit_nth_msf (set->data [i], BITS_PER_CHUNK) + i * BITS_PER_CHUNK;
	}
	return -1;
}

/*
 * mono_bitset_find_first_unset:
 * @set: bitset ptr
 * @pos: pos to search _after_ (not including)
 *
 * Returns position of first unset bit after @pos. If pos < 0 begin search from
 * start. Return -1 if no bit set is found.
 */
int
mono_bitset_find_first_unset (const MonoBitSet *set, gint pos) {
	int j;
	int bit;
	int result, i;

	if (pos < 0) {
		j = 0;
		bit = -1;
	} else {
		j = pos / BITS_PER_CHUNK;
		bit = pos % BITS_PER_CHUNK;
		g_return_val_if_fail (pos < set->size, -1);
	}
	/*g_print ("find first from %d (j: %d, bit: %d)\n", pos, j, bit);*/

	if (set->data [j] != -1) {
		result = find_first_unset (set->data [j], bit);
		if (result != -1)
			return result + j * BITS_PER_CHUNK;
	}
	for (i = ++j; i < set->size / BITS_PER_CHUNK; ++i) {
		if (set->data [i] != -1) {
			return find_first_unset (set->data [i], -1) + i * BITS_PER_CHUNK;
		}
	}
	return -1;
}

/*
 * mono_bitset_clone:
 * @set: bitset ptr to clone
 * @new_size: number of bits the cloned bitset can hold
 *
 * Return a cloned bitset of size new_size. MONO_BITSET_DONT_FREE
 * unset in cloned bitset. If new_size is 0, the cloned object is just
 * as big.
 */
MonoBitSet*
mono_bitset_clone (const MonoBitSet *set, guint32 new_size) {
	MonoBitSet *result;

	if (!new_size)
		new_size = set->size;
	result = mono_bitset_new (new_size, set->flags);
	result->flags &= ~MONO_BITSET_DONT_FREE;
	memcpy (result->data, set->data, set->size / 8);
	return result;
}

/*
 * mono_bitset_copyto:
 * @src: bitset ptr to copy from
 * @dest: bitset ptr to copy to
 *
 * Copy one bitset to another.
 */
void
mono_bitset_copyto (const MonoBitSet *src, MonoBitSet *dest) {
	g_assert (dest->size <= src->size);

	memcpy (&dest->data, &src->data, dest->size / 8);
}

/*
 * mono_bitset_union:
 * @dest: bitset ptr to hold union
 * @src: bitset ptr to copy
 *
 * Make union of one bitset and another.
 */
void
mono_bitset_union (MonoBitSet *dest, const MonoBitSet *src) {
	int i, size;

	g_assert (src->size <= dest->size);

	size = dest->size / BITS_PER_CHUNK;
	for (i = 0; i < size; ++i)
		dest->data [i] |= src->data [i];
}

/*
 * mono_bitset_intersection:
 * @dest: bitset ptr to hold intersection
 * @src: bitset ptr to copy
 *
 * Make intersection of one bitset and another.
 */
void
mono_bitset_intersection (MonoBitSet *dest, const MonoBitSet *src) {
	int i, size;

	g_assert (src->size <= dest->size);

	size = dest->size / BITS_PER_CHUNK;
	for (i = 0; i < size; ++i)
		dest->data [i] &= src->data [i];
}

/*
 * mono_bitset_intersection_2:
 * @dest: bitset ptr to hold intersection
 * @src1: first bitset
 * @src2: second bitset
 *
 * Make intersection of two bitsets
 */
void
mono_bitset_intersection_2 (MonoBitSet *dest, const MonoBitSet *src1, const MonoBitSet *src2) {
	int i, size;

	g_assert (src1->size <= dest->size);
	g_assert (src2->size <= dest->size);

	size = dest->size / BITS_PER_CHUNK;
	for (i = 0; i < size; ++i)
		dest->data [i] = src1->data [i] & src2->data [i];
}

/*
 * mono_bitset_sub:
 * @dest: bitset ptr to hold bitset - src
 * @src: bitset ptr to copy
 *
 * Unset all bits in dest, which are set in src.
 */
void
mono_bitset_sub (MonoBitSet *dest, const MonoBitSet *src) {
	int i, size;

	g_assert (src->size <= dest->size);

	size = src->size / BITS_PER_CHUNK;
	for (i = 0; i < size; ++i)
		dest->data [i] &= ~src->data [i];
}

/*
 * mono_bitset_equal:
 * @src: bitset ptr
 * @src1: bitset ptr
 *
 * return TRUE if their size are the same and the same bits are set in
 * both bitsets.
 */
gboolean
mono_bitset_equal (const MonoBitSet *src, const MonoBitSet *src1) {
	int i;
	if (src->size != src1->size)
		return FALSE;

	for (i = 0; i < src->size / BITS_PER_CHUNK; ++i)
		if (src->data [i] != src1->data [i])
			return FALSE;
	return TRUE;
}

/*
 * mono_bitset_foreach:
 * @set: bitset ptr
 * @func: Function to call for every set bit
 * @data: pass this as second arg to func
 *
 * Calls func for every bit set in bitset. Argument 1 is the number of
 * the bit set, argument 2 is data
 */
void
mono_bitset_foreach (MonoBitSet *set, MonoBitSetFunc func, gpointer data)
{
	int i, j;
	for (i = 0; i < set->size / BITS_PER_CHUNK; ++i) {
		if (set->data [i]) {
			for (j = 0; j < BITS_PER_CHUNK; ++j)
				if (set->data [i] & ((gsize)1 << j))
					func (j + i * BITS_PER_CHUNK, data);
		}
	}
}

#ifdef TEST_BITSET

/*
 * Compile with: 
 * gcc -g -Wall -DTEST_BITSET -o monobitset monobitset.c `pkg-config --cflags --libs glib-2.0`
 */
int 
main() {
	MonoBitSet *set1, *set2, *set3, *set4;
	int error = 1;
	int count, i;

	set1 = mono_bitset_new (60, 0);
	set4 = mono_bitset_new (60, 0);

	if (mono_bitset_count (set1) != 0)
		return error;
	error++;
	
	mono_bitset_set (set1, 33);
	if (mono_bitset_count (set1) != 1)
		return error;
	error++;

	/* g_print("should be 33: %d\n", mono_bitset_find_first (set1, 0)); */
	
	if (mono_bitset_find_first (set1, 0) != 33)
		return error;
	error++;

	if (mono_bitset_find_first (set1, 33) != -1)
		return error;
	error++;

	/* test 5 */
	if (mono_bitset_find_first (set1, -100) != 33)
		return error;
	error++;

	if (mono_bitset_find_last (set1, -1) != 33)
		return error;
	error++;

	if (mono_bitset_find_last (set1, 33) != -1)
		return error;
	error++;

	if (mono_bitset_find_last (set1, -100) != 33)
		return error;
	error++;

	if (mono_bitset_find_last (set1, 34) != 33)
		return error;
	error++;

	/* test 10 */
	if (!mono_bitset_test (set1, 33))
		return error;
	error++;

	if (mono_bitset_test (set1, 32) || mono_bitset_test (set1, 34))
		return error;
	error++;

	set2 = mono_bitset_clone (set1, 0);
	if (mono_bitset_count (set2) != 1)
		return error;
	error++;

	mono_bitset_invert (set2);
	if (mono_bitset_count (set2) != (mono_bitset_size (set2) - 1))
		return error;
	error++;

	mono_bitset_clear (set2, 10);
	if (mono_bitset_count (set2) != (mono_bitset_size (set2) - 2))
		return error;
	error++;

	/* test 15 */
	set3 = mono_bitset_clone (set2, 0);
	mono_bitset_union (set3, set1);
	if (mono_bitset_count (set3) != (mono_bitset_size (set3) - 1))
		return error;
	error++;

	mono_bitset_clear_all (set2);
	if (mono_bitset_count (set2) != 0)
		return error;
	error++;

	mono_bitset_invert (set2);
	if (mono_bitset_count (set2) != mono_bitset_size (set2))
		return error;
	error++;

	mono_bitset_set (set4, 0);
	mono_bitset_set (set4, 1);
	mono_bitset_set (set4, 10);
	if (mono_bitset_count (set4) != 3)
		return error;
	error++;

	count = 0;
	for (i = mono_bitset_find_first (set4, -1); i != -1; i = mono_bitset_find_first (set4, i)) {
		count ++;
		switch (count) {
		case 1:
		  if (i != 0)
		    return error;
		  break;
		case 2:
		  if (i != 1)
		    return error;
		  break;
		case 3:
		  if (i != 10)
		    return error;
		  break;
		}
		/* g_print ("count got: %d at %d\n", count, i); */
	}
	if (count != 3)
		return error;
	error++;

	if (mono_bitset_find_first (set4, -1) != 0)
		return error;
	error++;

	/* 20 */
	mono_bitset_set (set4, 31);
	if (mono_bitset_find_first (set4, 10) != 31)
		return error;
	error++;

	mono_bitset_free (set1);

	set1 = mono_bitset_new (200, 0);
	mono_bitset_set (set1, 0);
	mono_bitset_set (set1, 1);
	mono_bitset_set (set1, 10);
	mono_bitset_set (set1, 31);
	mono_bitset_set (set1, 150);

	mono_bitset_free (set1);
	mono_bitset_free (set2);
	mono_bitset_free (set3);
	mono_bitset_free (set4);

	g_print ("total tests passed: %d\n", error - 1);
	
	return 0;
}

#endif
