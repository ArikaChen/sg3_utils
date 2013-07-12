/**
 * sg_hot_add
 *
 * Author: Sean Stewart <Sean.Stewart@netapp.com>
 * 
 * Utility for hot adding, removing, remapping detection of
 * SCSI devices
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "sg_lib.h"
#include "sg_cmds_basic.h"

#define DEF_RLUNS_BUFF_LEN (1024 * 8)

int get_lun_from_hctl( const char * hctl );

static const char * scsi_host = "/sys/class/scsi_host/";

struct target_lu {
};

struct dev_addr {
	int h;
	int c;
	int t;
	int l;
};

struct dev_addr filter;

int
get_lun_from_hctl( const char * hctl )
{
	int lun;

	if (!sscanf(hctl, "%*d:%*d:%*d:%d", &lun))
		return -1;
	else {
		return lun;
	}
}

static int
get_luns_by_target_select(const struct dirent * s)
{
	struct dev_addr s_target;

	if (!sscanf(s->d_name, "%d:%d:%d:%d\n", &s_target.h, &s_target.c, &s_target.t, &s_target.l)) {
		return 0;
	} else {		
		// Filter by the h:c:t
		if (s_target.h != filter.h ||
			s_target.c != filter.c ||
			s_target.t != filter.t)
			return 0;
	}

	return 1;
}

const char *
map_hctl_to_sg( const char * hctl )
{

}

static int
get_luns_by_target_sort(const struct dirent ** a, const struct dirent ** b)
{
	struct dev_addr addr_a, addr_b;
	int lun_a, lun_b;

	// Extract the LU number from each
	lun_a = get_lun_from_hctl((*(struct dirent **)a)->d_name);
	lun_b = get_lun_from_hctl((*(struct dirent **)b)->d_name);

	// Compare
	if (lun_a > lun_b)
		return -1;
	else if (lun_a < lun_b)
		return 1;
	else
		return 0;
}

// Pass h:c:t and get the list of scsi_devices seen on that target by the host
// TODO: We have to return this list like in the report luns.. with a reference to an array
int
get_luns_by_target( int host, int ctlr, int target, uint8_t **luns ) {
	struct dirent ** namelist;
	int n;
	int i;
	int lun, len;
	
	filter.h = host;
	filter.c = ctlr;
	filter.t = target;

	n = scandir("/sys/class/scsi_device", &namelist, get_luns_by_target_select, get_luns_by_target_sort);
	printf("n: %d\n", n);

	if(n < 0)
		return 0;
	else {
		len = n;
		*luns = (uint8_t *)calloc(1, n);
		printf("%x\n", *luns);
		for (i=0; i<len; i++) {
			n--;
			printf("before:  %s\n",namelist[n]->d_name);
			lun = get_lun_from_hctl(namelist[n]->d_name);
			printf("after get_lun: %d\n", lun);
			memcpy((*luns)+i, &lun, 1);
		
			free(namelist[n]);
		}
	}

	free(namelist);

	return len;	
}

int
report_luns( const char * dev, uint8_t **luns ) {
	int sg_fd, list_length, i;
	int max_len = DEF_RLUNS_BUFF_LEN;
	unsigned char * rlun_buffer = (unsigned char *)calloc(1, max_len);
	unsigned int offset;
	
	if( (sg_fd = sg_cmds_open_device(dev, 0, 3)) < 0) {
		free(rlun_buffer);
		return 0;
	}
	

	// Report LUNs from the device, dump the result in hex 
	sg_ll_report_luns(sg_fd, 0, rlun_buffer, max_len, 1, 3);
	list_length = (rlun_buffer[3]) + (rlun_buffer[2] << 8) + (rlun_buffer[1] << 16) + (rlun_buffer[0] << 24);
	dStrHex((const char *)rlun_buffer, (list_length+8), 1);

	*luns = (uint8_t *)calloc(1, (list_length/8));
	printf("%x\n", *luns);
	for (i = 0; i < (list_length/8); i++) {
		offset = ((i+1)*8)+1;
		memcpy((*luns)+i, &rlun_buffer[offset], 1);
	}

	sg_cmds_close_device(sg_fd);
	free(rlun_buffer);

	return (list_length/8);
}

int
main( int argc, char * argv[] ) {
	int i;
	uint8_t * test;
	uint8_t * test2;


// TODO: Create a data struct with the pointers and len and ability to compare them
	int len  = report_luns("/dev/sdac", &test);
	for (i=0; i<len; i++) {
		printf("Lun list: Entry %d is LUN%d\n", i, test[i]);
	}

	int len2 = get_luns_by_target(12, 0, 0, &test2);
	printf("AFTER %x\n", test2);
	for (i=0; i<len2; i++) {
		printf("Lun list: Entry %d is LUN%d\n", i, test2[i]);
	}

	free(test);

	return 0;
}

