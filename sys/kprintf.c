#include <sys/kprintf.h>
#include <sys/defs.h>
#include <sys/stdarg.h>
#include <sys/kmemcpy.h>

static  int count = 0;
static int colIndex = 0;
//static int rowIndex = 0;
static  char *tempMem = (char*)0xffffffff80000000 + 0xb8000;
#define X_AXIS	80
#define Y_AXIS	200
#define BUFOVFLOW	2
#define WRNGFMT	1
char buffer[Y_AXIS][X_AXIS];
int buffer_row = 0;
int buffer_col = 0;
int carriage_ret_flag = 0;


void shift_up() {
      int k;
      char *shiftPtr = (char*)0xffffffff80000000 + 0xb8000;
      for(k = 0; k < 23; k++) {
        memcpy(shiftPtr, shiftPtr + 160, 160);
        shiftPtr += 160;
      }
      for(k = 0; k < 80; k++) {
        *shiftPtr = ' ';
        shiftPtr = shiftPtr + 2;
      }
}

void local_echo() {
    int i = 0, j = 0;
    while(1) {
        j = colIndex;
        while(buffer[i][j] != '\0' && j < 80) {
                if(count >= 3840) {
                  shift_up();
                  tempMem = tempMem - 160;
                  count = count - 160;
                }  
		if (buffer[i][j] == '\b') {
			tempMem = tempMem - 2;
			j++;
			count = count - 2;
			*tempMem = ' ';
		}
		else {     
                	*tempMem = buffer[i][j];
                	j++;
                	tempMem = tempMem + 2;
                	count = count + 2;
		}
        }
        if(j >= 80) {
                colIndex = 0;
                i++;
        }
        else if(buffer[i][j] == '\0') break;
    }
    colIndex = (((uint64_t)tempMem - (0xffffffff80000000 + 0xb8000)) % 160) / 2;
}

int put_stdin_into_buffer(char c) {
        buffer_col = colIndex;
        buffer_row = 0;
        switch(c) {
        case '\n':
                buffer_row++;
                buffer_col = 0;
                break;
        case '\t':
                buffer_col = (buffer_col + 10) % 80;
                break;
        default:
                buffer[buffer_row][buffer_col] = c;
                buffer_col = (buffer_col + 1)%80;
                if (buffer_col == 0) buffer_row++;
                break;
        }
        buffer[buffer_row][buffer_col] = '\0';
        return 0;
}
void display() {
  int i = 0, j = 0; 
  if(carriage_ret_flag == 1) {
  	while( ((uint64_t)tempMem - (0xffffffff80000000 + 0xb8000))%160 != 0 ) {
 		*tempMem = ' ';
  		tempMem -= 2;
                count -=2;
        }
        carriage_ret_flag = 0;
  }

  while(1) {
        j = colIndex;
  	while(buffer[i][j] != '\0' && j < 80) {
		if(count >= 3840) {
		  shift_up();
		  tempMem = tempMem - 160;
		  count = count - 160;
		}
		*tempMem = buffer[i][j];
		j++;
		tempMem = tempMem + 2;
		count = count + 2;
        }
        if(j >= 80) {
		colIndex = 0;
		i++;
	}
        else if(buffer[i][j] == '\0') break;
  }
  colIndex = (((uint64_t)tempMem - (0xffffffff80000000 + 0xb8000)) % 160) / 2;
}



char *get_ptr(uint64_t num, char *str) {
	if(num == 0) {
                str[0] = '0';
                str[1]= '\0';
                return str;
        }
        str[18] = '\0';
        int shift = 4;
        int64_t mask = 0xf;
        int str_index = 17;
        while ((str_index >= 2) && (num != 0)) {
                int val = (num & mask);
                char c;
                if (val > 9) c = val + 87;
                else    c = val + '0';
                str[str_index--] = c;
                num = (num & (~mask)) >> shift;
        }
        char *newstr = str+str_index+1;
        return newstr;
}
//19
char *process_ptr(uint64_t num, char *str) {
	char *str2 = get_ptr(num,str);
        *(--str2) = 'x';
        *(--str2) = '0';
	return str2;	
}
//9
char *process_hex(int64_t num, char *str) {
        if(num == 0) {
                str[0] = '0';
                str[1]= '\0';
                return str;
        }
        str[8] = '\0';
	int shift = 4;
        int64_t mask = 0xf;
        int str_index = 7;
        while ((str_index >= 0) && (num != 0)) {
		int val = (num & mask);
                char c;
                if (val > 9) c = val + 87;
                else    c = val + '0';
                str[str_index--] = c;
                num = (num & (~mask)) >> shift;
        }
        char *newstr = str+str_index+1;
        return newstr;
}

//12
char *process_int(int n, char *str) {
        int64_t num = n;
        if (num == 0) {
                str[0] = '0';
                str[1] = '\0';
                return str;
        }
        str[11] = '\0';
        int str_index = 10;
        int is_neg = 0;
        if (num < 0) {
                is_neg = 1;
                num = 0 - num;
        }
        while (num > 0) {
                char c = (num % 10) + '0';
                num = num / 10;
                str[str_index--] = c;
        }
        if (is_neg) {
                str[str_index--] = '-';
        }
        char *finstr = str + str_index + 1;
        return finstr;
}

int put_char_into_buffer(char c) {
	/*
	 * stuff to handle :
	 * 1) \n \t
	 * 2) updating bufferrow
	 * corner cases: \t might push it to the next line
	 */
	// to clarify, buffer_row and buffer_col needs to be updated to the 
	// next char that is to be written
	if (buffer_row >=200)
		return BUFOVFLOW;
	switch(c) {
	case '\n':
		buffer_row++;
		buffer_col = 0;
		break;
	case '\t':
		buffer_col = (buffer_col + 4) % 80;
		break;
	case '\v':
		buffer_row++;
		break;
	case '\r':
		if (buffer_row == 0) {
			carriage_ret_flag = 1;
		}
		while(buffer_col != 0) {
			buffer[buffer_row][buffer_col]= ' ';
			buffer_col--;
		}
		buffer[buffer_row][0] = ' ';
		buffer_col = 0;
		break;
	default:// normal chars
		buffer[buffer_row][buffer_col] = c;
		buffer_col = (buffer_col + 1)%80;
		if (buffer_col == 0) buffer_row++;
		break;
	}
	return 0;
		
}
int put_str_into_buffer(char *str) {
	int err=0;
	while(*str) {
		err = put_char_into_buffer(*str);
		if (err)
			return err;
		str++;	
	}
	return 0;	
}
void init_buffer() {
	int i,j;
	for(i=0;i<200;i++)
		for(j=0;j<80;j++)
			buffer[i][j]= ' ';
	return;
}
void kprintf(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	const char *tempfmt=fmt;
	int error=0;
	init_buffer();
        buffer_col = colIndex;
	buffer_row = 0;
	while (*tempfmt != '\0') {
		if (*tempfmt == '%' && *(tempfmt+1)) {
			char *str = NULL;
			switch(*(tempfmt+1)) {
			case 's':
				str = va_arg(args, char *);
				error = put_str_into_buffer(str);
				break;
			case 'c':;
				char charval;
				charval = va_arg(args, int);
				error = put_char_into_buffer(charval);
				break;
			case 'x':;
				int hexval;
				hexval = va_arg(args, int);
				char strhex[9];
                                str = process_hex(hexval, strhex);
				error = put_str_into_buffer(str);
				break;
			case 'p':;
				uint64_t ptrval;
				ptrval = (uint64_t)va_arg(args, void *);
				char strptr[19];
                                str = process_ptr(ptrval, strptr);
				error = put_str_into_buffer(str);
				break;
			case 'd':;
				int intval;
				intval = va_arg(args, int);
				char strint[12];
		                str = process_int(intval, strint);
				error = put_str_into_buffer(str);
				break;
			case '%':
				error = put_char_into_buffer(*(tempfmt+1));
				break;
			default:error = WRNGFMT;
				break;
			}
			if (error != 0)
				break;
			tempfmt += 2;
		}
		else {
//			printf("%c\n",*tempfmt);
			error = put_char_into_buffer(*tempfmt);
			if (error)
                                break;
			tempfmt += 1;
		}
		
	}
	if (buffer_row != Y_AXIS) {
//		printf("check null %d %d\n",buffer_row,buffer_col);
		buffer[buffer_row][buffer_col] = '\0';
	}
	//handle error here, wrong format and buffer overflow	
	va_end(args);	
	display();
}
