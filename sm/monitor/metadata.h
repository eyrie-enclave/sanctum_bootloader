#ifndef MONITOR_METADATA_H_INCLUDED
#define MONITOR_METADATA_H_INCLUDED

#include <arch/base_types.h>
#include <arch/atomics.h>
#include <public/api.h>

// Metadata regions are DRAM regions that are dedicated to storing the metadata
// used by the monitor to manage enclaves. The metadata is not stored inside
// enclave regions so that the monitor can read or modify the metadata without
// impacting an enclave's LLC lines.
//
// Each metadata region is managed as a collection of pages. The first few
// pages store an array of metadata_page_info_t elements, and the other pages
// are usable for metadata storage. Each metadata_page_info_t element indicates
// the ownership and data type of its corresponding metadata page.
//
// For simplicity, the monitor implementation assumes that the
// metadata_page_info_t array fits into a single DRAM regon stripe. The boot
// initialization sequence ensures that the invariant holds.

// The page map entry for a metadata page is packed in a single pointer.
//
// This is possible because an enclave ID points to the enclave's metadata
// page, so each enclave ID is page-aligned. This makes the bottom bits
// available for storing accounting information.
typedef uintptr_t metadata_page_info_t;

// Mask that selects the metadata page type bits.
#define metadata_page_type_mask 3

// Type for metadata pages that have been assigned to enclaves but not used.
//
// These pages are waiting to be turned into threat_info_t pages by the enclave
// software.
#define empty_metadata_page_type 0

// Type for metadata pages that store inner pages for data structures.
//
// The first page of each data structure has a type that identifies the data
// structure. The other pages use the inner type. This way, we can check that
// an address points to the beginning of a metadata structure by comparing the
// page's type against the expected structure type.
#define inner_metadata_page_type 1

// Type for metadata pages that hold an enclave's enclave_info_t.
#define enclave_metadata_page_type 2

// Type for metadata pages that hold a thread_info_t for an enclave.
#define thread_metadata_page_type 3

// Total number of metadata pages in a DRAM region dedicated to metadata.
extern size_t g_metadata_region_pages;

// The first usable metadata page in a DRAM region dedicated to metadata.
//
// Unusable pages contain a map of all the pages, ensuring that metadata pages
// aren't double-allocated.
extern size_t g_metadata_region_start;

#endif  // !defined(MONITOR_METADATA_H_INCLUDED)
