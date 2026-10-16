// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PAVEL TKACHEV
/* Experimental RG34XXSP vendor adapter. No kernel text or USB-mode writes. */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/utsname.h>
#include <linux/mutex.h>
#include "vendor_profile.h"
extern void set_os_sleep_mode(int);
extern int get_os_sleep_mode(void);
static bool ready, wake_on_open;
static DEFINE_MUTEX(policy_lock);
static int set_wake(const char *value, const struct kernel_param *param)
{
 bool requested;int result=kstrtobool(value,&requested);
 if(result)return result;
 mutex_lock(&policy_lock);
 if(!ready) {mutex_unlock(&policy_lock);return -EAGAIN;}
 /* Userspace must write os_sleep=16 first, then verify the hall wake attribute. */
 if(get_os_sleep_mode()!=1) {mutex_unlock(&policy_lock);return -EINVAL;}
 set_os_sleep_mode(requested?0:16);
 wake_on_open=requested;
 pr_info("rondo_lid_wake: requested=%d usb_mode=%d\n",requested,get_os_sleep_mode());
 mutex_unlock(&policy_lock);
 return 0;
}
static const struct kernel_param_ops wake_ops={.set=set_wake,.get=param_get_bool};
module_param_cb(wake_on_open,&wake_ops,&wake_on_open,0600);
MODULE_PARM_DESC(wake_on_open,"Hall wake policy after os_sleep=16; experimental RG34XXSP only");
static int __init lid_init(void)
{
 BUILD_BUG_ON(offsetof(struct uts_namespace,name)+offsetof(struct new_utsname,release)!=134);
 BUILD_BUG_ON(offsetof(struct uts_namespace,name)+offsetof(struct new_utsname,version)!=199);
 if(strcmp(init_utsname()->release,VENDOR_RELEASE) || strcmp(init_utsname()->version,VENDOR_VERSION) ||
    memcmp((const void *)set_os_sleep_mode,vendor_set_code,sizeof(vendor_set_code)))
  return -ENODEV;
 ready=true;
 pr_info("rondo_lid_wake: audited vendor adapter ready; policy unchanged\n");
 return 0;
}
static void __exit lid_exit(void)
{
 mutex_lock(&policy_lock);ready=false;
 set_os_sleep_mode(get_os_sleep_mode());
 mutex_unlock(&policy_lock);
}
module_init(lid_init);
module_exit(lid_exit);
MODULE_LICENSE("Dual MIT/GPL");
MODULE_DESCRIPTION("Experimental independent lid wake for the audited RG34XXSP kernel");
