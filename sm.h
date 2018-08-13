#ifndef SM
#define SM

// SM Types
enum sm_region_state_t{Free, Owned, Blocked};
enum sm_thread_state{Free, Assigned, Initialized, Scheduled};
enum sm_mailbox_state{Empty, Full};
enum sm_page_type{Invalid, Enclave, Thread};

typedef sm_page_map_entry_t uint64_t;
typedef sm_page_map_t sm_page_map_entry_t[8192];
typedef mailbox_t uint8_t[4096];
typedef hash_t uint8_t[64];
typedef hart_state_t uint64_t[31]; // TODO: also include fp registers, if applicable?

typedef struct sm_enclave_info_t {
  uint8_t is_sealed;
  uint8_t is_debuggable;
  uint16_t num_mailboxes;
  uint16_t num_threads_scheduled;
  mailbox_t* mailbox;
  uint64_t region_bitmap;
  hash_t measurement_hash;
};

typedef struct sm_enclave_thread_state_t {
  uint32_t lock;
  uint32_t aex_state_valid;
  uint64_t host_pc;
  uint64_t host_sp;
  uint64_t enclave_pt_base;
  uint64_t enclave_entry_pc;
  uint64_t enclave_entry_sp;
  uint64_t enclave_fault_handler_pc;
  uint64_t enclave_fault_handler_sp;
  hart_state_t fault_state;
  hart_state_t aex_state;
};

#endif
