#include "t/prolog.h"

int main(int argc,char **argv) 
{
	int r;

	r = vmlog_tag_from_name("enter",5);
	IS(r,VMLOG_TAG_ENTER);
    
	r = vmlog_tag_from_name("leave",5);
	IS(r,VMLOG_TAG_LEAVE);
    
	r = vmlog_tag_from_name("throw",5);
	IS(r,VMLOG_TAG_THROW);
    
	r = vmlog_tag_from_name("catch",5);
	IS(r,VMLOG_TAG_CATCH);
    
	r = vmlog_tag_from_name("unwnd",5);
	IS(r,VMLOG_TAG_UNWND);
    
	r = vmlog_tag_from_name(NULL,0);
	IS(r,-1);
    
	r = vmlog_tag_from_name(NULL,5);
	IS(r,-1);
    
	r = vmlog_tag_from_name("enter",-1);
	IS(r,-1);

	r = vmlog_tag_from_name("enter",0);
	IS(r,-1);
    
	r = vmlog_tag_from_name("enter",4);
	IS(r,-1);
	
	r = vmlog_tag_from_name("enter",6);
	IS(r,-1);

	finished();
}

/* vim: noet ts=8 sw=8
 */
