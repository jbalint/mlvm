This is a new implementation of the LVM:

	1) two stream unification
	2) new set of instructions
	3) new CGC and memory management
	4) multi-threading version with consideration of mobile agents

Tag consideration:

	There are two kinds of tags in the LVM system: tag_on_pointer
and tag_on_value. REF, LIS, STR and FLP are tag_on_pointer while
CON, INT are tag_on_value.

	Tag_on_pointer must use two bits as the tag, because most 32-bit
machines and OS's adpot allign-4 scheme for address such that the first
30 bits represent the effective address. In this case, it is a risk to
use 3-bit tag because shifting an address (1 bit to the left) might lose 
the most significant bit (which might be 1) and therefore causing a wrong
address manipulation.

	As there are only four 2-bit permutations and we have to reserve
one permutation to tag_on_value terms, we are restricted to use three
permutations to represent our tag_on_pointer terms.

Last 2-bit permulations:

tag_on_pointer:

	00	REF
	01	LIS
	10	STR or FLP
		where STR -> 	FUN
						term_1
						...
						term_n

			  FLP ->	FLV
						word_1
						word_2

tag_on_value:

	11	INT, CON, FLV and FUN

		where INT	011	 where 29 bit value integer arithmetic
			  CON 	111  where
							first 9 bits for arity if 0 then CON
							else FUN
							next 20 bits for name table index
							FVL is a special FUN defined as
								0xfffffffff

The reason of setting up different tags for terms is to make memory
expansion and pointer adjustment easier. For example, after a round
of memory expansion, we only need to adjust tag_on_pointer terms,
and leave tag_on_value terms unchanged. A special case is FLV term
where two successive words following the FLV represent a double value.
Since these two words could be arbitrary bit sequences, we have to
jump over them during pointer adjustment phase (this is also the
reason why we need a special functor FLV for a floating value).


For example, let "from" be the old block pointer and "to" the new block
pointer,

		ax = *from++;
		if (is_con(ax)){
			*to++ = ax;
			if (is_flv(ax)){
				*to++ = *from++;
				*to++ = *from++;
			}
 		}
		else{
			bx = tag(ax);
			*to++ = (int)(addr(ax) + adjustment) | bx;
		}

The above code involves both move and adjust, however, if we use realloc
to expand the memory, then only adjustment is required.

And then following the CF chain to adjust TT fields, and re-adjust  CP,
following BB chain to re-adjust BP.

Note: if code and stack share a single memory block, then CP and BP do not
need re-adjustment.

									
