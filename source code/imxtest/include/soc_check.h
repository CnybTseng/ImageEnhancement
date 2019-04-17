#define false	0
#define	true	1
#define	LEN	10

/*
 * @soc_name: soc list to be checked
 * return: true-match,false-not match
 */
int  soc_version_check(char **soc_name);

int  soc_version_check(char **soc_name)
{
	int ret;
	FILE *soc_fd;
	char name[LEN];
	char **list = soc_name;

	soc_fd = fopen("/sys/devices/soc0/soc_id", "r");
	if (soc_fd == NULL)
		return false;
	ret = fread(name, 1, LEN, soc_fd);
	fclose(soc_fd);

	if (ret <= 0)
		return false;

	name[ret-1] = '\0';
	while (**list != ' ') {
		if (!strcmp(name, *list))
			goto out;
		else
			list++;
	}

out:
	if (**list != ' ')
		return true;
	else
		return false;
}
