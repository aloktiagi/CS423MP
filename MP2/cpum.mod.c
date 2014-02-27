#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x84b3ef83, "module_layout" },
	{ 0xccee136b, "kmem_cache_destroy" },
	{ 0x1cb9af37, "kmalloc_caches" },
	{ 0x12da5bb2, "__kmalloc" },
	{ 0xc996d097, "del_timer" },
	{ 0x2359df1b, "find_vpid" },
	{ 0x2ad04077, "remove_proc_entry" },
	{ 0x593a99b, "init_timer_key" },
	{ 0xcd76c5bb, "mutex_unlock" },
	{ 0x91715312, "sprintf" },
	{ 0x7d11c268, "jiffies" },
	{ 0x45a79637, "proc_mkdir" },
	{ 0x50eedeb8, "printk" },
	{ 0x20c55ae0, "sscanf" },
	{ 0xb4390f9a, "mcount" },
	{ 0xb36b32ce, "kmem_cache_free" },
	{ 0x1b8479a3, "mutex_lock" },
	{ 0x8834396c, "mod_timer" },
	{ 0xfb745572, "pid_task" },
	{ 0x218b2db7, "kmem_cache_alloc" },
	{ 0x3bd1b1f6, "msecs_to_jiffies" },
	{ 0xd78b6795, "create_proc_entry" },
	{ 0x43ff78f1, "kmem_cache_alloc_trace" },
	{ 0x3bcbf383, "kmem_cache_create" },
	{ 0x37a0cba, "kfree" },
	{ 0x362ef408, "_copy_from_user" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "63C0620B0974FBCE0A796C4");
