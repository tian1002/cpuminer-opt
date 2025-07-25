#include "argon2d-gate.h"
#include "simd-utils.h"
#include "argon2d/argon2.h"

static const size_t INPUT_BYTES = 80;  // Lenth of a block header in bytes. Input Length = Salt Length (salt = input)
static const size_t OUTPUT_BYTES = 32; // Length of output needed for a 256-bit hash
static const unsigned int DEFAULT_ARGON2_FLAG = 2; //Same as ARGON2_DEFAULT_FLAGS

// generic, works with most variations of argon2d
int scanhash_argon2d( struct work *work, uint32_t max_nonce,
                      uint64_t *hashes_done, struct thr_info *mythr )
{
   uint32_t _ALIGN(64) edata[20];
   uint32_t _ALIGN(64) hash[8];
   uint32_t *pdata = work->data;
   uint32_t *ptarget = work->target;
   const int thr_id = mythr->id;
   const uint32_t first_nonce = (const uint32_t)pdata[19];
   const uint32_t last_nonce = (const uint32_t)max_nonce;
   uint32_t nonce = first_nonce;
   const bool bench = opt_benchmark;

   v128_bswap32_80( edata, pdata );
   do
   {
      edata[19] = nonce;
      algo_gate.hash( hash, edata, thr_id );
      if ( unlikely( valid_hash( hash, ptarget ) && !bench ) )
      {
          pdata[19] = bswap_32( nonce );
          submit_solution( work, hash, mythr );
      }
      nonce++;
  } while ( likely( nonce < last_nonce && !work_restart[thr_id].restart ) );

   pdata[19] = nonce;
   *hashes_done = pdata[19] - first_nonce;
   return 0;
}

void argon2d250_hash( void *output, const void *input )
{
	argon2_context context;
	context.out = (uint8_t *)output;
	context.outlen = (uint32_t)OUTPUT_BYTES;
	context.pwd = (uint8_t *)input;
	context.pwdlen = (uint32_t)INPUT_BYTES;
	context.salt = (uint8_t *)input; //salt = input
	context.saltlen = (uint32_t)INPUT_BYTES;
	context.secret = NULL;
	context.secretlen = 0;
	context.ad = NULL;
	context.adlen = 0;
	context.allocate_cbk = NULL;
	context.free_cbk = NULL;
	context.flags = DEFAULT_ARGON2_FLAG; // = ARGON2_DEFAULT_FLAGS
	// main configurable Argon2 hash parameters
	context.m_cost = 250; // Memory in KiB (~256KB)
	context.lanes = 4;    // Degree of Parallelism
	context.threads = 1;  // Threads
	context.t_cost = 1;   // Iterations
        context.version = ARGON2_VERSION_10;

	argon2_ctx( &context, Argon2_d );
}

bool register_argon2d250_algo( algo_gate_t* gate )
{
        gate->scanhash = (void*)&scanhash_argon2d;
        gate->hash = (void*)&argon2d250_hash;
        opt_target_factor = 65536.0;
        return true;
}

void argon2d500_hash( void *output, const void *input )
{
    argon2_context context;
    context.out = (uint8_t *)output;
    context.outlen = (uint32_t)OUTPUT_BYTES;
    context.pwd = (uint8_t *)input;
    context.pwdlen = (uint32_t)INPUT_BYTES;
    context.salt = (uint8_t *)input; //salt = input
    context.saltlen = (uint32_t)INPUT_BYTES;
    context.secret = NULL;
    context.secretlen = 0;
    context.ad = NULL;
    context.adlen = 0;
    context.allocate_cbk = NULL;
    context.free_cbk = NULL;
    context.flags = DEFAULT_ARGON2_FLAG; // = ARGON2_DEFAULT_FLAGS
    // main configurable Argon2 hash parameters
    context.m_cost = 500;  // Memory in KiB (512KB)
    context.lanes = 8;     // Degree of Parallelism
    context.threads = 1;   // Threads
    context.t_cost = 2;    // Iterations
    context.version = ARGON2_VERSION_10;

    argon2_ctx( &context, Argon2_d );
}

bool register_argon2d500_algo( algo_gate_t* gate )
{
        gate->scanhash = (void*)&scanhash_argon2d;
        gate->hash = (void*)&argon2d500_hash;
        opt_target_factor = 65536.0;
        return true;
}

void argon2d1000_hash( void *output, const void *input )
{
    argon2_context context;
    context.out = (uint8_t *)output;
    context.outlen = (uint32_t)OUTPUT_BYTES;
    context.pwd = (uint8_t *)input;
    context.pwdlen = (uint32_t)INPUT_BYTES;
    context.salt = (uint8_t *)input; //salt = input
    context.saltlen = (uint32_t)INPUT_BYTES;
    context.secret = NULL;
    context.secretlen = 0;
    context.ad = NULL;
    context.adlen = 0;
    context.allocate_cbk = NULL;
    context.free_cbk = NULL;
    context.flags = DEFAULT_ARGON2_FLAG; // = ARGON2_DEFAULT_FLAGS
    // main configurable Argon2 hash parameters
    context.m_cost = 1000;  // Memory in KiB (1MB)
    context.lanes = 8;     // Degree of Parallelism
    context.threads = 1;   // Threads
    context.t_cost = 2;    // Iterations
    context.version = ARGON2_VERSION_10;

    argon2_ctx( &context, Argon2_d );
}

bool register_argon2d1000_algo( algo_gate_t* gate )
{
        gate->scanhash = (void*)&scanhash_argon2d;
        gate->hash = (void*)&argon2d1000_hash;
        opt_target_factor = 65536.0;
        return true;
}

void argon2d16000_hash( void *output, const void *input )
{
   argon2_context context;
   context.out = (uint8_t *)output;
   context.outlen = (uint32_t)OUTPUT_BYTES;
   context.pwd = (uint8_t *)input;
   context.pwdlen = (uint32_t)INPUT_BYTES;
   context.salt = (uint8_t *)input; //salt = input
   context.saltlen = (uint32_t)INPUT_BYTES;
   context.secret = NULL;
   context.secretlen = 0;
   context.ad = NULL;
   context.adlen = 0;
   context.allocate_cbk = NULL;
   context.free_cbk = NULL;
   context.flags = DEFAULT_ARGON2_FLAG; // = ARGON2_DEFAULT_FLAGS
   // main configurable Argon2 hash parameters
   context.m_cost = 16000; // Memory in KiB (~16384KB)
   context.lanes = 1;    // Degree of Parallelism
   context.threads = 1;  // Threads
   context.t_cost = 1;   // Iterations
   context.version = ARGON2_VERSION_10;

   argon2_ctx( &context, Argon2_d );
}

bool register_argon2d16000_algo( algo_gate_t* gate )
{
        gate->scanhash = (void*)&scanhash_argon2d;
        gate->hash = (void*)&argon2d16000_hash;
        opt_target_factor = 65536.0;
        return true;
}

int scanhash_argon2d4096( struct work *work, uint32_t max_nonce,
                           uint64_t *hashes_done, struct thr_info *mythr )
{
   uint32_t _ALIGN(64) vhash[8];
   uint32_t _ALIGN(64) edata[20];
   uint32_t *pdata = work->data;
   uint32_t *ptarget = work->target;
   const uint32_t first_nonce = pdata[19];
   const uint32_t last_nonce = (const uint32_t)max_nonce;
   uint32_t n = first_nonce;
   const int thr_id = mythr->id;  
   uint32_t t_cost = 1; // 1 iteration
   uint32_t m_cost = 4096; // use 4MB
   uint32_t parallelism = 1; // 1 thread, 2 lanes
   const bool bench = opt_benchmark;

   v128_bswap32_80( edata, pdata );

   do {
      edata[19] = n;
      argon2d_hash_raw( t_cost, m_cost, parallelism, (char*) edata, 80,
                 (char*) edata, 80, (char*) vhash, 32, ARGON2_VERSION_13 );
      if ( unlikely( valid_hash( vhash, ptarget ) && !bench ) )
      {
         be32enc( &pdata[19], n );
         submit_solution( work, vhash, mythr );
      }
      n++;
   } while ( likely( n < last_nonce && !work_restart[thr_id].restart ) );

   *hashes_done = n - first_nonce;
   pdata[19] = n;
   return 0;
}

bool register_argon2d4096_algo( algo_gate_t* gate )
{
        gate->scanhash = (void*)&scanhash_argon2d4096;
        opt_target_factor = 65536.0;
        return true;
}

