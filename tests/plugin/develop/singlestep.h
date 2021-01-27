

/**
 * init_singlestep_req
 *
 * Init singlestep module. This will initialise all needed variables
 */
void init_singlestep_req();

/**
 * check_singlestep
 *
 * Check weather singlestepping should be enabled or not. It will disable singlestep if no requestes are open. If requests are open it will force qemu into singlestep.
 */
void check_singlestep();

/**
 * add_singlestep_req
 *
 * Increase counter for requested singlesteps. This function should be called, if singlestep should be enabled. It will internally call check_singlestep
 */
void add_singlestep_req();

/**
 * rem_singlestep_req
 *
 * decrease counter for request singlestep. This function should be called, if singlestep should be disabled or is no longer needed. 
 */
void rem_singlestep_req();
