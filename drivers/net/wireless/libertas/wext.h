/**
  * This file contains definition for IOCTL call.
  */
#ifndef	_LBS_WEXT_H_
#define	_LBS_WEXT_H_

extern struct iw_handler_def lbs_handler_def;
extern struct iw_handler_def mesh_handler_def;
#if 1 /* TERRY(2010-0330): Periodic check of signal strength */
void lbs_stats_worker(struct work_struct *work);
#endif

#endif
