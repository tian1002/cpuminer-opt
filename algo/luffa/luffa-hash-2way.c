#include <string.h>
#include "luffa-hash-2way.h"
#include <stdio.h>

#if defined(__AVX2__)

#include "simd-utils.h"

#define uint32 uint32_t

/* initial values of chaining variables */
static const uint32_t IV[40] __attribute((aligned(64))) = {
    0xdbf78465,0x4eaa6fb4,0x44b051e0,0x6d251e69,
    0xdef610bb,0xee058139,0x90152df4,0x6e292011,
    0xde099fa3,0x70eee9a0,0xd9d2f256,0xc3b44b95,
    0x746cd581,0xcf1ccf0e,0x8fc944b3,0x5d9b0557,
    0xad659c05,0x04016ce5,0x5dba5781,0xf7efc89d,
    0x8b264ae7,0x24aa230a,0x666d1836,0x0306194f,
    0x204b1f67,0xe571f7d7,0x36d79cce,0x858075d5,
    0x7cde72ce,0x14bcb808,0x57e9e923,0x35870c6a,
    0xaffb4363,0xc825b7c7,0x5ec41e22,0x6c68e9be,
    0x03e86cea,0xb07224cc,0x0fc688f1,0xf5df3999
};

/* Round Constants */
static const uint32_t CNS_INIT[128] __attribute((aligned(64))) = {
    0xb213afa5,0xfc20d9d2,0xb6de10ed,0x303994a6,
    0xe028c9bf,0xe25e72c1,0x01685f3d,0xe0337818,
    0xc84ebe95,0x34552e25,0x70f47aae,0xc0e65299,
    0x44756f91,0xe623bb72,0x05a17cf4,0x441ba90d,
    0x4e608a22,0x7ad8818f,0x0707a3d4,0x6cc33a12,
    0x7e8fce32,0x5c58a4a4,0xbd09caca,0x7f34d442,
    0x56d858fe,0x8438764a,0x1c1e8f51,0xdc56983e,
    0x956548be,0x1e38e2e7,0xf4272b28,0x9389217f,
    0x343b138f,0xbb6de032,0x707a3d45,0x1e00108f,
    0xfe191be2,0x78e38b9d,0x144ae5cc,0xe5a8bce6,
    0xd0ec4e3d,0xedb780c8,0xaeb28562,0x7800423d,
    0x3cb226e5,0x27586719,0xfaa7ae2b,0x5274baf4,
    0x2ceb4882,0xd9847356,0xbaca1589,0x8f5b7882,
    0x5944a28e,0x36eda57f,0x2e48f1c1,0x26889ba7,
    0xb3ad2208,0xa2c78434,0x40a46f3e,0x96e1db12,
    0xa1c4c355,0x703aace7,0xb923c704,0x9a226e9d,
    0x00000000,0x00000000,0x00000000,0xf0d2e9e3,
    0x00000000,0x00000000,0x00000000,0x5090d577,
    0x00000000,0x00000000,0x00000000,0xac11d7fa,
    0x00000000,0x00000000,0x00000000,0x2d1925ab,
    0x00000000,0x00000000,0x00000000,0x1bcb66f2,
    0x00000000,0x00000000,0x00000000,0xb46496ac,
    0x00000000,0x00000000,0x00000000,0x6f2d9bc9,
    0x00000000,0x00000000,0x00000000,0xd1925ab0,
    0x00000000,0x00000000,0x00000000,0x78602649,
    0x00000000,0x00000000,0x00000000,0x29131ab6,
    0x00000000,0x00000000,0x00000000,0x8edae952,
    0x00000000,0x00000000,0x00000000,0x0fc053c3,
    0x00000000,0x00000000,0x00000000,0x3b6ba548,
    0x00000000,0x00000000,0x00000000,0x3f014f0c,
    0x00000000,0x00000000,0x00000000,0xedae9520,
    0x00000000,0x00000000,0x00000000,0xfc053c31
};


#if defined(SIMD512)

#define cns4w(i)  mm512_bcast_m128( ( (__m128i*)CNS_INIT)[i] )

#define ADD_CONSTANT4W( a, b, c0, c1 ) \
    a = _mm512_xor_si512( a, c0 ); \
    b = _mm512_xor_si512( b, c1 );

#define MULT24W( a0, a1 ) \
{ \
  __m512i b = _mm512_xor_si512( a0, \
                     _mm512_maskz_shuffle_epi32( 0xbbbb, a1, 0x10 ) ); \
  a0 = _mm512_alignr_epi8( a1,  b, 4 ); \
  a1 = _mm512_alignr_epi8(  b, a1, 4 ); \
}

#define SUBCRUMB4W( a0, a1, a2, a3 ) \
{ \
    __m512i t = a0; \
    a0 = mm512_xoror( a3, a0, a1 ); \
    a2 = _mm512_xor_si512( a2, a3 ); \
    a1 = _mm512_ternarylogic_epi64( a1, a3, t, 0x87 ); /* a1 xnor (a3 & t) */ \
    a3 = mm512_xorand( a2, a3, t ); \
    a2 = mm512_xorand( a1, a2, a0); \
    a1 = _mm512_or_si512( a1, a3 ); \
    a3 = _mm512_xor_si512( a3, a2 ); \
    t  = _mm512_xor_si512( t, a1 ); \
    a2 = _mm512_and_si512( a2, a1 ); \
    a1 = mm512_xnor( a1, a0 ); \
    a0 = t; \
}

#define MIXWORD4W( a, b ) \
    b = _mm512_xor_si512( a, b ); \
    a = _mm512_xor_si512( b, _mm512_rol_epi32( a, 2 ) ); \
    b = _mm512_xor_si512( a, _mm512_rol_epi32( b, 14 ) ); \
    a = _mm512_xor_si512( b, _mm512_rol_epi32( a, 10 ) ); \
    b = _mm512_rol_epi32( b, 1 );

#define STEP_PART4W( x0, x1, x2, x3, x4, x5, x6, x7, c0, c1 ) \
    SUBCRUMB4W( x0, x1, x2, x3 ); \
    SUBCRUMB4W( x5, x6, x7, x4 ); \
    MIXWORD4W( x0, x4 ); \
    MIXWORD4W( x1, x5 ); \
    MIXWORD4W( x2, x6 ); \
    MIXWORD4W( x3, x7 ); \
    ADD_CONSTANT4W( x0, x4, c0, c1 );

#define STEP_PART24W( a0, a1, t0, t1, c0, c1 ) \
    t0 = _mm512_shuffle_epi32( a1, 147 ); \
    a1 = _mm512_unpacklo_epi32( t0, a0 ); \
    t0 = _mm512_unpackhi_epi32( t0, a0 ); \
    t1 = _mm512_shuffle_epi32( t0, 78 ); \
    a0 = _mm512_shuffle_epi32( a1, 78 ); \
    SUBCRUMB4W( t1, t0, a0, a1 ); \
    t0 = _mm512_unpacklo_epi32( t0, t1 ); \
    a1 = _mm512_unpacklo_epi32( a1, a0 ); \
    a0 = _mm512_unpackhi_epi64( a1, t0 ); \
    a1 = _mm512_unpacklo_epi64( a1, t0 ); \
    a1 = _mm512_shuffle_epi32( a1, 57 ); \
    MIXWORD4W( a0, a1 ); \
    ADD_CONSTANT4W( a0, a1, c0, c1 );

#define NMLTOM10244W(r0,r1,r2,r3,s0,s1,s2,s3,p0,p1,p2,p3,q0,q1,q2,q3)\
    s1 = _mm512_unpackhi_epi32( r3, r2 ); \
    q1 = _mm512_unpackhi_epi32( p3, p2 ); \
    s3 = _mm512_unpacklo_epi32( r3, r2 ); \
    q3 = _mm512_unpacklo_epi32( p3, p2 ); \
    r3 = _mm512_unpackhi_epi32( r1, r0 ); \
    r1 = _mm512_unpacklo_epi32( r1, r0 ); \
    p3 = _mm512_unpackhi_epi32( p1, p0 ); \
    p1 = _mm512_unpacklo_epi32( p1, p0 ); \
    s0 = _mm512_unpackhi_epi64( s1, r3 ); \
    q0 = _mm512_unpackhi_epi64( q1 ,p3 ); \
    s1 = _mm512_unpacklo_epi64( s1, r3 ); \
    q1 = _mm512_unpacklo_epi64( q1, p3 ); \
    s2 = _mm512_unpackhi_epi64( s3, r1 ); \
    q2 = _mm512_unpackhi_epi64( q3, p1 ); \
    s3 = _mm512_unpacklo_epi64( s3, r1 ); \
    q3 = _mm512_unpacklo_epi64( q3, p1 );

#define MIXTON10244W(r0,r1,r2,r3,s0,s1,s2,s3,p0,p1,p2,p3,q0,q1,q2,q3)\
    NMLTOM10244W(r0,r1,r2,r3,s0,s1,s2,s3,p0,p1,p2,p3,q0,q1,q2,q3);

void rnd512_4way( luffa_4way_context *state, const __m512i *msg )
{
    __m512i t0, t1;
    __m512i *chainv = state->chainv;
    __m512i x0, x1, x2, x3, x4, x5, x6, x7;

    t0 = mm512_xor3( chainv[0], chainv[2], chainv[4] );
    t1 = mm512_xor3( chainv[1], chainv[3], chainv[5] );
    t0 = mm512_xor3( t0, chainv[6], chainv[8] );
    t1 = mm512_xor3( t1, chainv[7], chainv[9] );

    MULT24W( t0, t1 );

    chainv[0] = _mm512_xor_si512( chainv[0], t0 );
    chainv[1] = _mm512_xor_si512( chainv[1], t1 );
    chainv[2] = _mm512_xor_si512( chainv[2], t0 );
    chainv[3] = _mm512_xor_si512( chainv[3], t1 );
    chainv[4] = _mm512_xor_si512( chainv[4], t0 );
    chainv[5] = _mm512_xor_si512( chainv[5], t1 );
    chainv[6] = _mm512_xor_si512( chainv[6], t0 );
    chainv[7] = _mm512_xor_si512( chainv[7], t1 );
    chainv[8] = _mm512_xor_si512( chainv[8], t0 );
    chainv[9] = _mm512_xor_si512( chainv[9], t1 );

    t0 = chainv[0];
    t1 = chainv[1];

    MULT24W( chainv[0], chainv[1] );
    chainv[0] = _mm512_xor_si512( chainv[0], chainv[2] );
    chainv[1] = _mm512_xor_si512( chainv[1], chainv[3] );

    MULT24W( chainv[2], chainv[3] );
    chainv[2] = _mm512_xor_si512(chainv[2], chainv[4]);
    chainv[3] = _mm512_xor_si512(chainv[3], chainv[5]);

    MULT24W( chainv[4], chainv[5] );
    chainv[4] = _mm512_xor_si512(chainv[4], chainv[6]);
    chainv[5] = _mm512_xor_si512(chainv[5], chainv[7]);

    MULT24W( chainv[6], chainv[7] );
    chainv[6] = _mm512_xor_si512(chainv[6], chainv[8]);
    chainv[7] = _mm512_xor_si512(chainv[7], chainv[9]);

    MULT24W( chainv[8], chainv[9] );
    t0 = chainv[8] = _mm512_xor_si512( chainv[8], t0 );
    t1 = chainv[9] = _mm512_xor_si512( chainv[9], t1 );

    MULT24W( chainv[8], chainv[9] );
    chainv[8] = _mm512_xor_si512( chainv[8], chainv[6] );
    chainv[9] = _mm512_xor_si512( chainv[9], chainv[7] );

    MULT24W( chainv[6], chainv[7] );
    chainv[6] = _mm512_xor_si512( chainv[6], chainv[4] );
    chainv[7] = _mm512_xor_si512( chainv[7], chainv[5] );

    MULT24W( chainv[4], chainv[5] );
    chainv[4] = _mm512_xor_si512( chainv[4], chainv[2] );
    chainv[5] = _mm512_xor_si512( chainv[5], chainv[3] );

    MULT24W( chainv[2], chainv[3] );
    chainv[2] = _mm512_xor_si512( chainv[2], chainv[0] );
    chainv[3] = _mm512_xor_si512( chainv[3], chainv[1] );

    MULT24W( chainv[0], chainv[1] );
    chainv[0] = _mm512_xor_si512( chainv[0], t0 );
    chainv[1] = _mm512_xor_si512( chainv[1], t1 );

    if ( msg )
    {
       __m512i msg0, msg1;

       msg0 = _mm512_shuffle_epi32( msg[0], 27 );
       msg1 = _mm512_shuffle_epi32( msg[1], 27 );

       chainv[0] = _mm512_xor_si512( chainv[0], msg0 );
       chainv[1] = _mm512_xor_si512( chainv[1], msg1 );

       MULT24W( msg0, msg1 );
       chainv[2] = _mm512_xor_si512( chainv[2], msg0 );
       chainv[3] = _mm512_xor_si512( chainv[3], msg1 );

       MULT24W( msg0, msg1 );
       chainv[4] = _mm512_xor_si512( chainv[4], msg0 );
       chainv[5] = _mm512_xor_si512( chainv[5], msg1 );

       MULT24W( msg0, msg1 );
       chainv[6] = _mm512_xor_si512( chainv[6], msg0 );
       chainv[7] = _mm512_xor_si512( chainv[7], msg1 );

       MULT24W( msg0, msg1);
       chainv[8] = _mm512_xor_si512( chainv[8], msg0 );
       chainv[9] = _mm512_xor_si512( chainv[9], msg1 );
    }
    
    chainv[3] = _mm512_rol_epi32( chainv[3], 1 );
    chainv[5] = _mm512_rol_epi32( chainv[5], 2 );
    chainv[7] = _mm512_rol_epi32( chainv[7], 3 );
    chainv[9] = _mm512_rol_epi32( chainv[9], 4 );

    NMLTOM10244W( chainv[0], chainv[2], chainv[4], chainv[6], x0, x1, x2, x3,
                  chainv[1], chainv[3], chainv[5], chainv[7], x4, x5, x6, x7 );

    STEP_PART4W( x0, x1, x2, x3, x4, x5, x6, x7, cns4w( 0), cns4w( 1) );
    STEP_PART4W( x0, x1, x2, x3, x4, x5, x6, x7, cns4w( 2), cns4w( 3) );
    STEP_PART4W( x0, x1, x2, x3, x4, x5, x6, x7, cns4w( 4), cns4w( 5) );
    STEP_PART4W( x0, x1, x2, x3, x4, x5, x6, x7, cns4w( 6), cns4w( 7) );
    STEP_PART4W( x0, x1, x2, x3, x4, x5, x6, x7, cns4w( 8), cns4w( 9) );
    STEP_PART4W( x0, x1, x2, x3, x4, x5, x6, x7, cns4w(10), cns4w(11) );
    STEP_PART4W( x0, x1, x2, x3, x4, x5, x6, x7, cns4w(12), cns4w(13) );
    STEP_PART4W( x0, x1, x2, x3, x4, x5, x6, x7, cns4w(14), cns4w(15) );

    MIXTON10244W( x0, x1, x2, x3, chainv[0], chainv[2], chainv[4], chainv[6],
                  x4, x5, x6, x7, chainv[1], chainv[3], chainv[5], chainv[7] );

    /* Process last 256-bit block */
    STEP_PART24W( chainv[8], chainv[9], t0, t1, cns4w(16), cns4w(17) );
    STEP_PART24W( chainv[8], chainv[9], t0, t1, cns4w(18), cns4w(19) );
    STEP_PART24W( chainv[8], chainv[9], t0, t1, cns4w(20), cns4w(21) );
    STEP_PART24W( chainv[8], chainv[9], t0, t1, cns4w(22), cns4w(23) );
    STEP_PART24W( chainv[8], chainv[9], t0, t1, cns4w(24), cns4w(25) );
    STEP_PART24W( chainv[8], chainv[9], t0, t1, cns4w(26), cns4w(27) );
    STEP_PART24W( chainv[8], chainv[9], t0, t1, cns4w(28), cns4w(29) );
    STEP_PART24W( chainv[8], chainv[9], t0, t1, cns4w(30), cns4w(31) );
}

void finalization512_4way( luffa_4way_context *state, uint32 *b )
{
    uint32_t hash[8*4] __attribute((aligned(128)));
    __m512i* chainv = state->chainv;
    __m512i t[2];

    /*---- blank round with m=0 ----*/
    rnd512_4way( state, NULL );
    
    t[0] = mm512_xor3( chainv[0], chainv[2], chainv[4] );
    t[1] = mm512_xor3( chainv[1], chainv[3], chainv[5] );
    t[0] = mm512_xor3( t[0], chainv[6], chainv[8] );
    t[1] = mm512_xor3( t[1], chainv[7], chainv[9] );
    t[0] = _mm512_shuffle_epi32( t[0], 27 );
    t[1] = _mm512_shuffle_epi32( t[1], 27 );

    _mm512_store_si512( (__m512i*)&hash[ 0], t[0] );
    _mm512_store_si512( (__m512i*)&hash[16], t[1] );

    casti_m512i( b,0 ) = mm512_bswap_32( casti_m512i( hash,0 ) );
    casti_m512i( b,1 ) = mm512_bswap_32( casti_m512i( hash,1 ) );

    rnd512_4way( state, NULL );

    t[0] = mm512_xor3( chainv[0], chainv[2], chainv[4] );
    t[1] = mm512_xor3( chainv[1], chainv[3], chainv[5] );
    t[0] = mm512_xor3( t[0], chainv[6], chainv[8] );
    t[1] = mm512_xor3( t[1], chainv[7], chainv[9] );
    t[0] = _mm512_shuffle_epi32( t[0], 27 );
    t[1] = _mm512_shuffle_epi32( t[1], 27 );

    _mm512_store_si512( (__m512i*)&hash[ 0], t[0] );
    _mm512_store_si512( (__m512i*)&hash[16], t[1] );

    casti_m512i( b,2 ) = mm512_bswap_32( casti_m512i( hash,0 ) );
    casti_m512i( b,3 ) = mm512_bswap_32( casti_m512i( hash,1 ) );
}

int luffa_4way_init( luffa_4way_context *state, int hashbitlen )
{
    state->hashbitlen = hashbitlen;
    __m128i *iv = (__m128i*)IV;

    state->chainv[0] = mm512_bcast_m128( iv[0] );
    state->chainv[1] = mm512_bcast_m128( iv[1] );
    state->chainv[2] = mm512_bcast_m128( iv[2] );
    state->chainv[3] = mm512_bcast_m128( iv[3] );
    state->chainv[4] = mm512_bcast_m128( iv[4] );
    state->chainv[5] = mm512_bcast_m128( iv[5] );
    state->chainv[6] = mm512_bcast_m128( iv[6] );
    state->chainv[7] = mm512_bcast_m128( iv[7] );
    state->chainv[8] = mm512_bcast_m128( iv[8] );
    state->chainv[9] = mm512_bcast_m128( iv[9] );

    ((__m512i*)state->buffer)[0] = m512_zero;
    ((__m512i*)state->buffer)[1] = m512_zero;

    return 0;
}

int luffa512_4way_init( luffa_4way_context *state )
{
   return luffa_4way_init( state, 512 );
}
   
// Do not call luffa_update_close after having called luffa_update.
// Once luffa_update has been called only call luffa_update or luffa_close.
int luffa_4way_update( luffa_4way_context *state, const void *data,
                       size_t len )
{
    __m512i *vdata  = (__m512i*)data;
    __m512i *buffer = (__m512i*)state->buffer;
    __m512i msg[2];
    int i;
    int blocks = (int)len >> 5;

    state->rembytes = (int)len & 0x1F;

    // full blocks
    for ( i = 0; i < blocks; i++, vdata+=2 )
    {
       msg[0] = mm512_bswap_32( vdata[ 0 ] );
       msg[1] = mm512_bswap_32( vdata[ 1 ] );
       rnd512_4way( state, msg );
    }

    // 16 byte partial block exists for 80 byte len
    // store in buffer for transform in final for midstate to work
    if ( state->rembytes  )
    {
      // remaining data bytes
      buffer[0] = mm512_bswap_32( vdata[0] );
      buffer[1] = mm512_bcast128lo_64( 0x0000000080000000 );
    }
    return 0;
}

/*
int luffa512_4way_update( luffa_4way_context *state, const void *data,
                       size_t len )
{
   return luffa_4way_update( state, data, len );
}
*/

int luffa_4way_close( luffa_4way_context *state, void *hashval )
{
    __m512i *buffer = (__m512i*)state->buffer;
    __m512i msg[2];

    // transform pad block
    if ( state->rembytes )
      // not empty, data is in buffer
      rnd512_4way( state, buffer );
    else
    {     // empty pad block, constant data
      msg[0] = mm512_bcast128lo_64( 0x0000000080000000 );
      msg[1] = m512_zero;
      rnd512_4way( state, msg );
    }
    finalization512_4way( state, (uint32*)hashval );

    if ( state->hashbitlen > 512 )
        finalization512_4way( state, (uint32*)( hashval+32 ) );
    return 0;
}

/*
int luffa512_4way_close( luffa_4way_context *state, void *hashval )
{
   return luffa_4way_close( state, hashval );
}
*/

int luffa512_4way_full( luffa_4way_context *state, void *output,
                        const void *data, size_t inlen )
{
    state->hashbitlen = 512;
    __m128i *iv = (__m128i*)IV;

    state->chainv[0] = mm512_bcast_m128( iv[0] );
    state->chainv[1] = mm512_bcast_m128( iv[1] );
    state->chainv[2] = mm512_bcast_m128( iv[2] );
    state->chainv[3] = mm512_bcast_m128( iv[3] );
    state->chainv[4] = mm512_bcast_m128( iv[4] );
    state->chainv[5] = mm512_bcast_m128( iv[5] );
    state->chainv[6] = mm512_bcast_m128( iv[6] );
    state->chainv[7] = mm512_bcast_m128( iv[7] );
    state->chainv[8] = mm512_bcast_m128( iv[8] );
    state->chainv[9] = mm512_bcast_m128( iv[9] );

    ((__m512i*)state->buffer)[0] = m512_zero;
    ((__m512i*)state->buffer)[1] = m512_zero;

    const __m512i *vdata  = (__m512i*)data;
    __m512i msg[2];
    int i;
    const int blocks = (int)( inlen >> 5 );

    state->rembytes = inlen & 0x1F;

    // full blocks
    for ( i = 0; i < blocks; i++, vdata+=2 )
    {
       msg[0] = mm512_bswap_32( vdata[ 0 ] );
       msg[1] = mm512_bswap_32( vdata[ 1 ] );
       rnd512_4way( state, msg );
    }

    // 16 byte partial block exists for 80 byte len
    if ( state->rembytes  )
    {
       // padding of partial block
       msg[0] = mm512_bswap_32( vdata[ 0 ] );
       msg[1] = mm512_bcast128lo_64( 0x0000000080000000 );
       rnd512_4way( state, msg );
    }
    else
    {
       // empty pad block
       msg[0] = mm512_bcast128lo_64( 0x0000000080000000 );
       msg[1] = m512_zero;
       rnd512_4way( state, msg );
    }

    finalization512_4way( state, (uint32*)output );

    if ( state->hashbitlen > 512 )
        finalization512_4way( state, (uint32*)( output+64 ) );

    return 0;
}

int luffa_4way_update_close( luffa_4way_context *state,
                 void *output, const void *data, size_t inlen )
{
// Optimized for integrals of 16 bytes, good for 64 and 80 byte len
    const __m512i *vdata  = (__m512i*)data;
    __m512i msg[2];
    int i;
    const int blocks = (int)( inlen >> 5 );

    state->rembytes = inlen & 0x1F;

    // full blocks
    for ( i = 0; i < blocks; i++, vdata+=2 )
    {
       msg[0] = mm512_bswap_32( vdata[ 0 ] );
       msg[1] = mm512_bswap_32( vdata[ 1 ] );
       rnd512_4way( state, msg );
    }

    // 16 byte partial block exists for 80 byte len
    if ( state->rembytes  )
    {
       // padding of partial block
       msg[0] = mm512_bswap_32( vdata[ 0 ] );
       msg[1] = mm512_bcast128lo_64( 0x0000000080000000 );
       rnd512_4way( state, msg );
    }
    else
    {
       // empty pad block
       msg[0] = mm512_bcast128lo_64( 0x0000000080000000 );
       msg[1] = m512_zero;
       rnd512_4way( state, msg );
    }

    finalization512_4way( state, (uint32*)output );

    if ( state->hashbitlen > 512 )
        finalization512_4way( state, (uint32*)( output+64 ) );

    return 0;
}

#endif // AVX512

#define cns(i)  mm256_bcast_m128( ( (__m128i*)CNS_INIT)[i] )

#define ADD_CONSTANT( a, b, c0, c1 ) \
    a = _mm256_xor_si256( a, c0 ); \
    b = _mm256_xor_si256( b, c1 );

#if defined(VL256) 

#define MULT2( a0, a1 ) \
{ \
  __m256i b = _mm256_xor_si256( a0, \
                     _mm256_maskz_shuffle_epi32( 0xbb, a1, 0x10 ) ); \
  a0 = _mm256_alignr_epi8( a1,  b, 4 ); \
  a1 = _mm256_alignr_epi8(  b, a1, 4 ); \
}

#define SUBCRUMB( a0, a1, a2, a3 ) \
{ \
    __m256i t = a0; \
    a0 = mm256_xoror( a3, a0, a1 ); \
    a2 = _mm256_xor_si256( a2, a3 ); \
    a1 = _mm256_ternarylogic_epi64( a1, a3, t, 0x87 ); /* a1 xnor (a3 & t) */ \
    a3 = mm256_xorand( a2, a3, t ); \
    a2 = mm256_xorand( a1, a2, a0); \
    a1 = _mm256_or_si256( a1, a3 ); \
    a3 = _mm256_xor_si256( a3, a2 ); \
    t  = _mm256_xor_si256( t, a1 ); \
    a2 = _mm256_and_si256( a2, a1 ); \
    a1 = mm256_xnor( a1, a0 ); \
    a0 = t; \
}

#else

#define MULT2( a0, a1 ) \
{ \
  __m256i b = _mm256_xor_si256( a0, _mm256_shuffle_epi32( \
                         _mm256_blend_epi32( a1, m256_zero, 0xee ), 0x10 ) ); \
  a0 = _mm256_alignr_epi8( a1,  b, 4 ); \
  a1 = _mm256_alignr_epi8(  b, a1, 4 ); \
}

#define SUBCRUMB( a0, a1, a2, a3 ) \
{ \
    __m256i t = a0; \
    a0 = _mm256_or_si256( a0, a1 ); \
    a2 = _mm256_xor_si256( a2, a3 ); \
    a1 = mm256_not( a1 ); \
    a0 = _mm256_xor_si256( a0, a3 ); \
    a3 = _mm256_and_si256( a3, t ); \
    a1 = _mm256_xor_si256( a1, a3 ); \
    a3 = _mm256_xor_si256( a3, a2 ); \
    a2 = _mm256_and_si256( a2, a0 ); \
    a0 = mm256_not( a0 ); \
    a2 = _mm256_xor_si256( a2, a1 ); \
    a1 = _mm256_or_si256(  a1, a3 ); \
    t  = _mm256_xor_si256(  t, a1 ); \
    a3 = _mm256_xor_si256( a3, a2 ); \
    a2 = _mm256_and_si256( a2, a1 ); \
    a1 = _mm256_xor_si256( a1, a0 ); \
    a0 = t; \
}

#endif

#define MIXWORD( a, b ) \
    b = _mm256_xor_si256( a, b ); \
    a = _mm256_xor_si256( b, mm256_rol_32( a,  2 ) ); \
    b = _mm256_xor_si256( a, mm256_rol_32( b, 14 ) ); \
    a = _mm256_xor_si256( b, mm256_rol_32( a, 10 ) ); \
    b = mm256_rol_32( b, 1 );

#define STEP_PART( x0, x1, x2, x3, x4, x5, x6, x7, c0, c1 ) \
    SUBCRUMB( x0, x1, x2, x3 ); \
    SUBCRUMB( x5, x6, x7, x4 ); \
    MIXWORD( x0, x4 ); \
    MIXWORD( x1, x5 ); \
    MIXWORD( x2, x6 ); \
    MIXWORD( x3, x7 ); \
    ADD_CONSTANT( x0, x4, c0, c1 );

#define STEP_PART2( a0, a1, t0, t1, c0, c1 ) \
    t0 = _mm256_shuffle_epi32( a1, 147 ); \
    a1 = _mm256_unpacklo_epi32( t0, a0 ); \
    t0 = _mm256_unpackhi_epi32( t0, a0 ); \
    t1 = _mm256_shuffle_epi32( t0, 78 ); \
    a0 = _mm256_shuffle_epi32( a1, 78 ); \
    SUBCRUMB( t1, t0, a0, a1 ); \
    t0 = _mm256_unpacklo_epi32( t0, t1 ); \
    a1 = _mm256_unpacklo_epi32( a1, a0 ); \
    a0 = _mm256_unpackhi_epi64( a1, t0 ); \
    a1 = _mm256_unpacklo_epi64( a1, t0 ); \
    a1 = _mm256_shuffle_epi32( a1, 57 ); \
    MIXWORD( a0, a1 ); \
    ADD_CONSTANT( a0, a1, c0, c1 );

#define NMLTOM1024(r0,r1,r2,r3,s0,s1,s2,s3,p0,p1,p2,p3,q0,q1,q2,q3)\
    s1 = _mm256_unpackhi_epi32( r3, r2 ); \
    q1 = _mm256_unpackhi_epi32( p3, p2 ); \
    s3 = _mm256_unpacklo_epi32( r3, r2 ); \
    q3 = _mm256_unpacklo_epi32( p3, p2 ); \
    r3 = _mm256_unpackhi_epi32( r1, r0 ); \
    r1 = _mm256_unpacklo_epi32( r1, r0 ); \
    p3 = _mm256_unpackhi_epi32( p1, p0 ); \
    p1 = _mm256_unpacklo_epi32( p1, p0 ); \
    s0 = _mm256_unpackhi_epi64( s1, r3 ); \
    q0 = _mm256_unpackhi_epi64( q1 ,p3 ); \
    s1 = _mm256_unpacklo_epi64( s1, r3 ); \
    q1 = _mm256_unpacklo_epi64( q1, p3 ); \
    s2 = _mm256_unpackhi_epi64( s3, r1 ); \
    q2 = _mm256_unpackhi_epi64( q3, p1 ); \
    s3 = _mm256_unpacklo_epi64( s3, r1 ); \
    q3 = _mm256_unpacklo_epi64( q3, p1 );

#define MIXTON1024(r0,r1,r2,r3,s0,s1,s2,s3,p0,p1,p2,p3,q0,q1,q2,q3)\
    NMLTOM1024(r0,r1,r2,r3,s0,s1,s2,s3,p0,p1,p2,p3,q0,q1,q2,q3);


/***************************************************/
/* Round function         */
/* state: hash context    */

void rnd512_2way( luffa_2way_context *state, const __m256i *msg )
{
    __m256i t0, t1;
    __m256i *chainv = state->chainv;
    __m256i x0, x1, x2, x3, x4, x5, x6, x7;

    t0 = mm256_xor3( chainv[0], chainv[2], chainv[4] );
    t1 = mm256_xor3( chainv[1], chainv[3], chainv[5] );
    t0 = mm256_xor3( t0, chainv[6], chainv[8] );
    t1 = mm256_xor3( t1, chainv[7], chainv[9] );

    MULT2( t0, t1 );

    chainv[0] = _mm256_xor_si256( chainv[0], t0 );
    chainv[1] = _mm256_xor_si256( chainv[1], t1 );
    chainv[2] = _mm256_xor_si256( chainv[2], t0 );
    chainv[3] = _mm256_xor_si256( chainv[3], t1 );
    chainv[4] = _mm256_xor_si256( chainv[4], t0 );
    chainv[5] = _mm256_xor_si256( chainv[5], t1 );
    chainv[6] = _mm256_xor_si256( chainv[6], t0 );
    chainv[7] = _mm256_xor_si256( chainv[7], t1 );
    chainv[8] = _mm256_xor_si256( chainv[8], t0 );
    chainv[9] = _mm256_xor_si256( chainv[9], t1 );

    t0 = chainv[0];
    t1 = chainv[1];

    MULT2( chainv[0], chainv[1] );
    chainv[0] = _mm256_xor_si256( chainv[0], chainv[2] );
    chainv[1] = _mm256_xor_si256( chainv[1], chainv[3] );

    MULT2( chainv[2], chainv[3] );
    chainv[2] = _mm256_xor_si256(chainv[2], chainv[4]);
    chainv[3] = _mm256_xor_si256(chainv[3], chainv[5]);

    MULT2( chainv[4], chainv[5] );
    chainv[4] = _mm256_xor_si256(chainv[4], chainv[6]);
    chainv[5] = _mm256_xor_si256(chainv[5], chainv[7]);

    MULT2( chainv[6], chainv[7] );
    chainv[6] = _mm256_xor_si256(chainv[6], chainv[8]);
    chainv[7] = _mm256_xor_si256(chainv[7], chainv[9]);

    MULT2( chainv[8], chainv[9] );
    t0 = chainv[8] = _mm256_xor_si256( chainv[8], t0 );
    t1 = chainv[9] = _mm256_xor_si256( chainv[9], t1 );

    MULT2( chainv[8], chainv[9] );
    chainv[8] = _mm256_xor_si256( chainv[8], chainv[6] );
    chainv[9] = _mm256_xor_si256( chainv[9], chainv[7] );

    MULT2( chainv[6], chainv[7] );
    chainv[6] = _mm256_xor_si256( chainv[6], chainv[4] );
    chainv[7] = _mm256_xor_si256( chainv[7], chainv[5] );

    MULT2( chainv[4], chainv[5] );
    chainv[4] = _mm256_xor_si256( chainv[4], chainv[2] );
    chainv[5] = _mm256_xor_si256( chainv[5], chainv[3] );

    MULT2( chainv[2], chainv[3] );
    chainv[2] = _mm256_xor_si256( chainv[2], chainv[0] );
    chainv[3] = _mm256_xor_si256( chainv[3], chainv[1] );

    MULT2( chainv[0], chainv[1] );
    chainv[0] = _mm256_xor_si256( chainv[0], t0 );
    chainv[1] = _mm256_xor_si256( chainv[1], t1 );

    if ( msg )
    {
       __m256i msg0, msg1;
    
       msg0 = _mm256_shuffle_epi32( msg[0], 27 );
       msg1 = _mm256_shuffle_epi32( msg[1], 27 );

       chainv[0] = _mm256_xor_si256( chainv[0], msg0 );
       chainv[1] = _mm256_xor_si256( chainv[1], msg1 );
    
       MULT2( msg0, msg1 );
       chainv[2] = _mm256_xor_si256( chainv[2], msg0 );
       chainv[3] = _mm256_xor_si256( chainv[3], msg1 );

       MULT2( msg0, msg1 );
       chainv[4] = _mm256_xor_si256( chainv[4], msg0 );
       chainv[5] = _mm256_xor_si256( chainv[5], msg1 );

       MULT2( msg0, msg1 );
       chainv[6] = _mm256_xor_si256( chainv[6], msg0 );
       chainv[7] = _mm256_xor_si256( chainv[7], msg1 );

       MULT2( msg0, msg1 );
       chainv[8] = _mm256_xor_si256( chainv[8], msg0 );
       chainv[9] = _mm256_xor_si256( chainv[9], msg1 );
    }

    chainv[3] = mm256_rol_32( chainv[3], 1 );
    chainv[5] = mm256_rol_32( chainv[5], 2 );
    chainv[7] = mm256_rol_32( chainv[7], 3 );
    chainv[9] = mm256_rol_32( chainv[9], 4 );

    NMLTOM1024( chainv[0], chainv[2], chainv[4], chainv[6], x0, x1, x2, x3,
                chainv[1], chainv[3], chainv[5], chainv[7], x4, x5, x6, x7 );

    STEP_PART( x0, x1, x2, x3, x4, x5, x6, x7, cns( 0), cns( 1) );
    STEP_PART( x0, x1, x2, x3, x4, x5, x6, x7, cns( 2), cns( 3) );
    STEP_PART( x0, x1, x2, x3, x4, x5, x6, x7, cns( 4), cns( 5) );
    STEP_PART( x0, x1, x2, x3, x4, x5, x6, x7, cns( 6), cns( 7) );
    STEP_PART( x0, x1, x2, x3, x4, x5, x6, x7, cns( 8), cns( 9) );
    STEP_PART( x0, x1, x2, x3, x4, x5, x6, x7, cns(10), cns(11) );
    STEP_PART( x0, x1, x2, x3, x4, x5, x6, x7, cns(12), cns(13) );
    STEP_PART( x0, x1, x2, x3, x4, x5, x6, x7, cns(14), cns(15) );

    MIXTON1024( x0, x1, x2, x3, chainv[0], chainv[2], chainv[4], chainv[6],
                x4, x5, x6, x7, chainv[1], chainv[3], chainv[5], chainv[7]);

    /* Process last 256-bit block */
    STEP_PART2( chainv[8], chainv[9], t0, t1, cns(16), cns(17) );
    STEP_PART2( chainv[8], chainv[9], t0, t1, cns(18), cns(19) );
    STEP_PART2( chainv[8], chainv[9], t0, t1, cns(20), cns(21) );
    STEP_PART2( chainv[8], chainv[9], t0, t1, cns(22), cns(23) );
    STEP_PART2( chainv[8], chainv[9], t0, t1, cns(24), cns(25) );
    STEP_PART2( chainv[8], chainv[9], t0, t1, cns(26), cns(27) );
    STEP_PART2( chainv[8], chainv[9], t0, t1, cns(28), cns(29) );
    STEP_PART2( chainv[8], chainv[9], t0, t1, cns(30), cns(31) );
}

/***************************************************/
/* Finalization function  */
/* state: hash context    */
/* b[8]: hash values      */

void finalization512_2way( luffa_2way_context *state, uint32 *b )
{
    uint32 hash[8*2] __attribute((aligned(64)));
    __m256i* chainv = state->chainv;
    __m256i t0, t1;
    /*---- blank round with m=0 ----*/
    rnd512_2way( state, NULL );

    t0 = mm256_xor3( chainv[0], chainv[2], chainv[4] );
    t1 = mm256_xor3( chainv[1], chainv[3], chainv[5] );
    t0 = mm256_xor3( t0, chainv[6], chainv[8] );
    t1 = mm256_xor3( t1, chainv[7], chainv[9] );

    t0 = _mm256_shuffle_epi32( t0, 27 );
    t1 = _mm256_shuffle_epi32( t1, 27 );

    _mm256_store_si256( (__m256i*)&hash[0], t0 );
    _mm256_store_si256( (__m256i*)&hash[8], t1 );

    casti_m256i( b, 0 ) = mm256_bswap_32( casti_m256i( hash, 0 ) );
    casti_m256i( b, 1 ) = mm256_bswap_32( casti_m256i( hash, 1 ) );

    rnd512_2way( state, NULL );

    t0 = mm256_xor3( chainv[0], chainv[2], chainv[4] );
    t1 = mm256_xor3( chainv[1], chainv[3], chainv[5] );
    t0 = mm256_xor3( t0, chainv[6], chainv[8] );
    t1 = mm256_xor3( t1, chainv[7], chainv[9] );
    
    t0 = _mm256_shuffle_epi32( t0, 27 );
    t1 = _mm256_shuffle_epi32( t1, 27 );

    _mm256_store_si256( (__m256i*)&hash[0], t0 );
    _mm256_store_si256( (__m256i*)&hash[8], t1 );

    casti_m256i( b, 2 ) = mm256_bswap_32( casti_m256i( hash, 0 ) );
    casti_m256i( b, 3 ) = mm256_bswap_32( casti_m256i( hash, 1 ) );
}

int luffa_2way_init( luffa_2way_context *state, int hashbitlen )
{
    state->hashbitlen = hashbitlen;
    __m128i *iv = (__m128i*)IV;
    
    state->chainv[0] = mm256_bcast_m128( iv[0] );
    state->chainv[1] = mm256_bcast_m128( iv[1] );
    state->chainv[2] = mm256_bcast_m128( iv[2] );
    state->chainv[3] = mm256_bcast_m128( iv[3] );
    state->chainv[4] = mm256_bcast_m128( iv[4] );
    state->chainv[5] = mm256_bcast_m128( iv[5] );
    state->chainv[6] = mm256_bcast_m128( iv[6] );
    state->chainv[7] = mm256_bcast_m128( iv[7] );
    state->chainv[8] = mm256_bcast_m128( iv[8] );
    state->chainv[9] = mm256_bcast_m128( iv[9] );

    ((__m256i*)state->buffer)[0] = m256_zero;
    ((__m256i*)state->buffer)[1] = m256_zero;

    return 0;
}

// Do not call luffa_update_close after having called luffa_update.
// Once luffa_update has been called only call luffa_update or luffa_close.
int luffa_2way_update( luffa_2way_context *state, const void *data,
                       size_t len )
{
    __m256i *vdata  = (__m256i*)data;
    __m256i *buffer = (__m256i*)state->buffer;
    __m256i msg[2];
    int i;
    int blocks = (int)len >> 5;
    state-> rembytes = (int)len & 0x1F;

    // full blocks
    for ( i = 0; i < blocks; i++, vdata+=2 )
    {
       msg[0] = mm256_bswap_32( vdata[ 0 ] );
       msg[1] = mm256_bswap_32( vdata[ 1 ] );
       rnd512_2way( state, msg );
    }

    // 16 byte partial block exists for 80 byte len
    // store in buffer for transform in final for midstate to work
    if ( state->rembytes  )
    {
      // remaining data bytes
      buffer[0] = mm256_bswap_32( vdata[0] );
      buffer[1] = mm256_bcast128lo_64( 0x0000000080000000 );
    }
    return 0;
}

int luffa_2way_close( luffa_2way_context *state, void *hashval )
{
    __m256i *buffer = (__m256i*)state->buffer;
    __m256i msg[2];

    // transform pad block
    if ( state->rembytes )
      // not empty, data is in buffer
      rnd512_2way( state, buffer );
    else
    {     // empty pad block, constant data
      msg[0] = mm256_bcast128lo_64( 0x0000000080000000 );
      msg[1] = m256_zero;
      rnd512_2way( state, msg );
    }
    finalization512_2way( state, (uint32*)hashval );

    if ( state->hashbitlen > 512 )
        finalization512_2way( state, (uint32*)( hashval+32 ) );
    return 0;
}

int luffa512_2way_full( luffa_2way_context *state, void *output,
                        const void *data, size_t inlen )
{
    state->hashbitlen = 512;
    __m128i *iv = (__m128i*)IV;

    state->chainv[0] = mm256_bcast_m128( iv[0] );
    state->chainv[1] = mm256_bcast_m128( iv[1] );
    state->chainv[2] = mm256_bcast_m128( iv[2] );
    state->chainv[3] = mm256_bcast_m128( iv[3] );
    state->chainv[4] = mm256_bcast_m128( iv[4] );
    state->chainv[5] = mm256_bcast_m128( iv[5] );
    state->chainv[6] = mm256_bcast_m128( iv[6] );
    state->chainv[7] = mm256_bcast_m128( iv[7] );
    state->chainv[8] = mm256_bcast_m128( iv[8] );
    state->chainv[9] = mm256_bcast_m128( iv[9] );

    ((__m256i*)state->buffer)[0] = m256_zero;
    ((__m256i*)state->buffer)[1] = m256_zero;

    const __m256i *vdata  = (__m256i*)data;
    __m256i msg[2];
    int i;
    const int blocks = (int)( inlen >> 5 );

    state->rembytes = inlen & 0x1F;

    // full blocks
    for ( i = 0; i < blocks; i++, vdata+=2 )
    {
       msg[0] = mm256_bswap_32( vdata[ 0 ] );
       msg[1] = mm256_bswap_32( vdata[ 1 ] );
       rnd512_2way( state, msg );
    }

    // 16 byte partial block exists for 80 byte len
    if ( state->rembytes  )
    {
       // padding of partial block
       msg[0] = mm256_bswap_32( vdata[ 0 ] );
       msg[1] = mm256_bcast128lo_64( 0x0000000080000000 );
       rnd512_2way( state, msg );
    }
    else
    {
       // empty pad block
       msg[0] = mm256_bcast128lo_64( 0x0000000080000000 );
       msg[1] = m256_zero;
       rnd512_2way( state, msg );
    }

    finalization512_2way( state, (uint32*)output );

    if ( state->hashbitlen > 512 )
        finalization512_2way( state, (uint32*)( output+32 ) );

    return 0;
}

int luffa_2way_update_close( luffa_2way_context *state,
                 void *output, const void *data, size_t inlen )
{
// Optimized for integrals of 16 bytes, good for 64 and 80 byte len
    const __m256i *vdata  = (__m256i*)data;
    __m256i msg[2];
    int i;
    const int blocks = (int)( inlen >> 5 );

    state->rembytes = inlen & 0x1F;

    // full blocks
    for ( i = 0; i < blocks; i++, vdata+=2 )
    {
       msg[0] = mm256_bswap_32( vdata[ 0 ] );
       msg[1] = mm256_bswap_32( vdata[ 1 ] );
       rnd512_2way( state, msg );
    }

    // 16 byte partial block exists for 80 byte len
    if ( state->rembytes  )
    {
       // padding of partial block
       msg[0] = mm256_bswap_32( vdata[ 0 ] );
       msg[1] = mm256_bcast128lo_64( 0x0000000080000000 );
       rnd512_2way( state, msg );
    }
    else
    {
       // empty pad block
       msg[0] = mm256_bcast128lo_64( 0x0000000080000000 );
       msg[1] = m256_zero;
       rnd512_2way( state, msg );
    }

    finalization512_2way( state, (uint32*)output );

    if ( state->hashbitlen > 512 )
        finalization512_2way( state, (uint32*)( output+32 ) );

    return 0;
}

#endif
