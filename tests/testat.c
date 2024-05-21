struct S
{
	int n;
	char text[16];
};

struct S a;
struct S v[10];

void f(char text[], int i, char ch)
{
	text[i] = ch;
	//return 1;
}

// if(v) ca nu e scalar
int h(int x, int y)
{
	

	if (x > 0 && x < y)
	{
		f(v[x].text, y, '#');
		// f(v[x].text, y);
		// f(2, 3, 4);
		

		return 1;
		// return "aaa";
	}
	return 0;
}