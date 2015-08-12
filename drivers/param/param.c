



#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <asm/unistd.h>
#include <asm/errno.h>
#include <asm/uaccess.h>

#include <param.h>


struct proc_dir_entry*	gp_param_proc_dir;


static char			sa_Param_dev_path[PARAM_DEV_PATH_LEN]	= { 0x0, };
static struct file*		sp_Param_device;
static mm_segment_t	ss_Old_FS;


static int param_get_dev_path(void)
{
#define DEFAULT_GPT_ENTRIES			128
#define MMCBLK_PART_INFO_PATH_LEN	128
#define PARTITION_NAME_LEN			128

	struct file*		p_file;
	mm_segment_t	s_Old_FS;
	char				a_part_info_path[MMCBLK_PART_INFO_PATH_LEN]	= { 0, };
	char				a_partition_name[PARTITION_NAME_LEN]			= { 0, };
	int				v_index;


	memset( sa_Param_dev_path, 0x0, PARAM_DEV_PATH_LEN );


	for( v_index = 0; v_index < DEFAULT_GPT_ENTRIES; v_index++ )
	{
		memset( a_part_info_path, 0x0, MMCBLK_PART_INFO_PATH_LEN );
		snprintf( a_part_info_path, MMCBLK_PART_INFO_PATH_LEN, "/sys/block/mmcblk0/mmcblk0p%d/partition_name", v_index + 1 );


		p_file	= filp_open( a_part_info_path, O_RDONLY, 0 );
		if( IS_ERR(p_file) )
		{
			PARAM_LOG( KERN_ERR "[%s] %s file open was failed!: %ld\n", __FUNCTION__, a_part_info_path, PTR_ERR(p_file) );
		}
		else
		{
			s_Old_FS	= get_fs();
			set_fs( get_ds() );


			memset( a_partition_name, 0x0, PARTITION_NAME_LEN );
			p_file->f_op->read( p_file, a_partition_name, PARTITION_NAME_LEN, &p_file->f_pos );


			set_fs( s_Old_FS );
			filp_close( p_file, NULL );


			/***
				Use the "strncmp" function to avoid following garbage character
			***/
			if( !strncmp( PARAM_PART_NAME, a_partition_name, strlen(PARAM_PART_NAME) ) )
			{
				snprintf( sa_Param_dev_path, PARAM_DEV_PATH_LEN, "/dev/block/mmcblk0p%d", v_index + 1 );
				PARAM_LOG( KERN_INFO "SEC_PARAM : %s device was found\n", sa_Param_dev_path );


				break;
			}
		}
	}


	if( sa_Param_dev_path[0] != 0x0 )
	{
		return	0;
	}
	else
	{
		return	-EFAULT;
	}
}

static int param_device_open(void)
{
	if( sa_Param_dev_path[0] == 0x0 )
	{
		if( !param_get_dev_path() )
		{
			PARAM_LOG( KERN_INFO "[%s] : %s partition was found for sec_param device.\n", __FUNCTION__, sa_Param_dev_path );
		}
		else
		{
			PARAM_LOG( KERN_ERR "[%s] : Can't find sec_param device!!!\n", __FUNCTION__ );
			return	-EFAULT;
		}
	}

	sp_Param_device	= filp_open( sa_Param_dev_path, O_RDWR|O_SYNC, 0 );
	if( IS_ERR(sp_Param_device) )
	{
		PARAM_LOG( KERN_ERR "[%s] param device(%s) open was failed!: %ld\n", __FUNCTION__, sa_Param_dev_path, PTR_ERR(sp_Param_device) );


		return	-EFAULT;
	}
	else
	{
		sp_Param_device->f_flags	|= O_NONBLOCK;
		ss_Old_FS	= get_fs();
		set_fs( get_ds() );


		return	0;
	}
}


static void param_device_close(void)
{
	set_fs( ss_Old_FS );
	sp_Param_device->f_flags	&= ~O_NONBLOCK;
	filp_close( sp_Param_device, NULL );

	return;
}





static int param_device_read(off_t v_offset, unsigned char* p_buffer, int v_size)
{
	int				v_ret;


	if( IS_ERR(sp_Param_device) )
	{
		PARAM_LOG( KERN_ERR "[%s] param device handler was fault!: %ld\n", __FUNCTION__, PTR_ERR(sp_Param_device) );


		return	-EFAULT;
	}
	else
	{
		v_ret	= sp_Param_device->f_op->llseek( sp_Param_device, PARAM_OFFSET + v_offset, SEEK_SET );
		if( v_ret < 0 )
		{
			v_ret	= -EPERM;
		}
		else
		{
			v_ret	= sp_Param_device->f_op->read( sp_Param_device, p_buffer, v_size, &sp_Param_device->f_pos );
		}


		return	v_ret;
	}
}






static int param_device_write(off_t v_offset, unsigned char* p_data, int v_size)
{
	int				v_ret;

	if( IS_ERR(sp_Param_device) )
	{
		PARAM_LOG( KERN_ERR "[%s] param device handler was fault!: %ld\n", __FUNCTION__, PTR_ERR(sp_Param_device) );


		return	-EFAULT;
	}
	else
	{
		v_ret	= sp_Param_device->f_op->llseek( sp_Param_device, PARAM_OFFSET + v_offset, SEEK_SET );
		if( v_ret < 0 )
		{
			v_ret	= -EPERM;
		}
		else
		{
			v_ret	= sp_Param_device->f_op->write( sp_Param_device, p_data, v_size, &sp_Param_device->f_pos );
		}

		return	v_ret;
	}
}


int sec_get_param(SEC_PARAM *buf)
{
	int				v_ret;
	
	v_ret	= param_device_open();
	if( v_ret )
	{
		return	-EFAULT;
	}
	
	memset( buf, 0xff, sizeof(SEC_PARAM) );
	
	v_ret	= param_device_read( 0, (unsigned char *)buf, sizeof(SEC_PARAM) );
	if( v_ret < 0 )
	{
		PARAM_LOG( KERN_ERR "[%s] : param read was failed!: %d\n", __FUNCTION__, v_ret );
		v_ret=-EFAULT;
	}
	
	param_device_close();
	return v_ret;
}


int sec_set_param(SEC_PARAM *buf)
{
	int				v_ret;
	
	v_ret	= param_device_open();
	if( v_ret )
	{
		return	-EFAULT;
	}
		
	v_ret	= param_device_write( 0, (unsigned char *)buf, sizeof(SEC_PARAM) );
	if( v_ret < 0 )
	{
		PARAM_LOG( KERN_ERR "[%s] : param write was failed!: %d\n", __FUNCTION__, v_ret );
		v_ret=-EFAULT;
	}
	
	param_device_close();
	return v_ret;
}


static ssize_t param_efs_info_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	int				v_ret;
	SEC_PARAM		s_efs	= { {0} };

	v_ret	= sec_get_param(&s_efs);
	if( v_ret < 0  )
		return	v_ret;
		
	PARAM_LOG( KERN_INFO "[%s] SEC_PARAM efs_info	  : %s\n", __FUNCTION__, s_efs.efs_info );

	return	sprintf( buf, "%s\n", s_efs.efs_info );
}


static ssize_t param_efs_info_write(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	int			v_ret;
	SEC_PARAM	s_efs;

	if( size < 1 )
		return	-EINVAL;

	if( size > sizeof(s_efs.efs_info) )
		return	-EFAULT;

	v_ret	= sec_get_param(&s_efs);
	if( v_ret < 0 )
		return	v_ret;

	memset( s_efs.efs_info, 0x0, sizeof(s_efs.efs_info) );
	memcpy( s_efs.efs_info, buf, (int)size );

	v_ret	= sec_set_param(&s_efs);
	if( v_ret < 0 )
		return	v_ret;

	return	size;
}
static DEVICE_ATTR(efs_info,0664, param_efs_info_read, param_efs_info_write);


static ssize_t param_fsbuild_check_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	int				v_ret;
	SEC_PARAM		s_efs	= { {0} };

	v_ret	= sec_get_param(&s_efs);
	if( v_ret < 0 )
		return	v_ret;
	
	PARAM_LOG( KERN_INFO "[%s] SEC_PARAM fsbuild_check	  : %s\n", __FUNCTION__, s_efs.fsbuild_check );

	return	sprintf( buf, "%s\n", s_efs.fsbuild_check );
}
 
static ssize_t param_fsbuild_check_write(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	int			v_ret;
	SEC_PARAM	s_efs;

	if( size < 1 )
		return	-EINVAL;


	if( size > sizeof(s_efs.fsbuild_check) )
		return	-EFAULT;

	v_ret	= sec_get_param(&s_efs);
	if( v_ret < 0 )
		return	v_ret;

	memset( s_efs.fsbuild_check, 0x0, sizeof(s_efs.fsbuild_check) );
	memcpy( s_efs.fsbuild_check, buf, (int)size );
	
	v_ret	= sec_set_param(&s_efs);
	if( v_ret < 0 )
		return	v_ret;

	return	size;
}

static DEVICE_ATTR(fsbuild_check,0664, param_fsbuild_check_read, param_fsbuild_check_write);



static ssize_t param_model_name_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	int				v_ret;
	SEC_PARAM		s_efs	= { {0} };

	v_ret	= sec_get_param(&s_efs);
	if( v_ret < 0 )
		return	v_ret;
	
	PARAM_LOG( KERN_INFO "[%s] SEC_PARAM model_name	  : %s\n", __FUNCTION__, s_efs.model_name );

	return	sprintf( buf, "%s\n", s_efs.model_name );
}
 
static ssize_t param_model_name_write(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	int			v_ret;
	SEC_PARAM	s_efs;

	if( size < 1 )
		return	-EINVAL;


	if( size > sizeof(s_efs.model_name) )
		return	-EFAULT;

	v_ret	= sec_get_param(&s_efs);
	if( v_ret < 0 )
		return	v_ret;

	memset( s_efs.model_name, 0x0, sizeof(s_efs.model_name) );
	memcpy( s_efs.model_name, buf, (int)size );

	v_ret	= sec_set_param(&s_efs);
	if( v_ret < 0 )
		return	v_ret;

	return	size;
}

static DEVICE_ATTR(model_name,0644, param_model_name_read, param_model_name_write);





static ssize_t param_sw_version_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	int				v_ret;
	SEC_PARAM		s_efs	= { {0} };

	v_ret	= sec_get_param(&s_efs);
	if( v_ret < 0 )
		return	v_ret;
	
	PARAM_LOG( KERN_INFO "[%s] SEC_PARAM sw_version	  : %s\n", __FUNCTION__, s_efs.sw_version );
		
	return	sprintf( buf, "%s\n", s_efs.sw_version );
}


static ssize_t param_sw_version_write(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	int			v_ret;
	SEC_PARAM	s_efs;

	if( size < 1 )
		return	-EINVAL;


	if( size > sizeof(s_efs.sw_version) )
		return	-EFAULT;

	v_ret	= sec_get_param(&s_efs);
	if( v_ret < 0 )
		return	v_ret;

	memset( s_efs.sw_version, 0x0, sizeof(s_efs.sw_version) );
	memcpy( s_efs.sw_version, buf, (int)size );

	v_ret	= sec_set_param(&s_efs);
	if( v_ret < 0 )
		return	v_ret;

	return	size;
}

static DEVICE_ATTR(sw_version,0644, param_sw_version_read, param_sw_version_write);






static ssize_t param_MD5_checksum_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	int				v_ret;
	SEC_PARAM		s_efs	= { {0} };

	v_ret	= sec_get_param(&s_efs);
	if( v_ret < 0 )
		return	v_ret;
		
	PARAM_LOG( KERN_INFO "[%s] SEC_PARAM MD5_checksum	  : %s\n", __FUNCTION__, s_efs.MD5_checksum );
		
	return	sprintf( buf, "%s\n", s_efs.MD5_checksum );
}

 
static ssize_t param_MD5_checksum_write(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	int			v_ret;
	SEC_PARAM	s_efs;

	if( size < 1 )
		return	-EINVAL;


	if( size > sizeof(s_efs.MD5_checksum) )
		return	-EFAULT;

	v_ret	= sec_get_param(&s_efs);
	if( v_ret < 0 )
		return	v_ret;

	memset( s_efs.MD5_checksum, 0x0, sizeof(s_efs.MD5_checksum) );
	memcpy( s_efs.MD5_checksum, buf, (int)size );

	v_ret	= sec_set_param(&s_efs);
	if( v_ret < 0 )
		return	v_ret;

	return	size;
}

static DEVICE_ATTR(MD5_checksum,0644, param_MD5_checksum_read, param_MD5_checksum_write);


static ssize_t param_recovery_opts_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	int				v_ret;
	SEC_PARAM		s_efs	= { {0} };

	v_ret	= sec_get_param(&s_efs);
	if( v_ret < 0 )
		return	v_ret;
		
	PARAM_LOG( KERN_INFO "[%s] SEC_PARAM recovery_opts	  : %s\n", __FUNCTION__, s_efs.recovery_opts );
		
	return	sprintf( buf, "%s\n", s_efs.recovery_opts );
}
 
static ssize_t param_recovery_opts_write(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	int			v_ret;
	SEC_PARAM	s_efs;

	if( size < 1 )
		return	-EINVAL;


	if( size > sizeof(s_efs.recovery_opts) )
		return	-EFAULT;

	v_ret	= sec_get_param(&s_efs);
	if( v_ret < 0 )
		return	v_ret;

	memset( s_efs.recovery_opts, 0x0, sizeof(s_efs.recovery_opts) );
	memcpy( s_efs.recovery_opts, buf, (int)size );
	
	v_ret	= sec_set_param(&s_efs);
	if( v_ret < 0 )
		return	v_ret;

	return	size;
}
static DEVICE_ATTR(recovery_opts,0664, param_recovery_opts_read, param_recovery_opts_write);

static struct device_attribute *sec_param_attrs[] = {
	&dev_attr_efs_info,
	&dev_attr_fsbuild_check,
	&dev_attr_model_name,
	&dev_attr_sw_version,
	&dev_attr_MD5_checksum,
	&dev_attr_recovery_opts,
};

static struct class *sec_param_class;
struct device *sec_param_dev;

static int __init param_init(void)
{
	int i, ret = 0;

	sec_param_class = class_create(THIS_MODULE, "sec_param");

	if (IS_ERR(sec_param_class)) {
		pr_err("Failed to create class(mdnie_class)!!\n");
		ret = -1;
		goto failed_create_class;
	}

	sec_param_dev = device_create(sec_param_class,
					NULL, 0, NULL, "sec_param");

	if (IS_ERR(sec_param_dev)) {
		printk(KERN_ERR "Failed to create device(sec_param_dev)!!");
		ret = -1;
		goto failed_create_dev;
	}

	for (i = 0; i < ARRAY_SIZE(sec_param_attrs) ; i++) {
		ret = device_create_file(sec_param_dev, sec_param_attrs[i]);
		if (ret < 0) {
			pr_err("failed to create device file - %d\n",i);
			ret = -1;
		}
	}

failed_create_class:
	return ret;

failed_create_dev:
	class_destroy(sec_param_class);
	return ret;	
}

module_init(param_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Samsung Param Operation");
