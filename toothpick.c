/*
 * https://en.wikipedia.org/wiki/Toothpick_sequence
 * https://www.youtube.com/watch?v=_UtCli1SgjI
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#ifdef __WIN64__
#include <windows.h>
#endif // __WIN64__

struct queue {
	size_t *data;
	size_t *front;
	size_t *end;
	size_t *data_end;
	size_t capacity;
};

struct pattern {
	uint8_t *data;
	struct queue endps;
	uint32_t width;
	uint32_t height;
	uint32_t iterations;
};

char *symbols[] = { " ", "╨", "╥", "║",
			"╡", "!", "!", "╣",
			"╞", "!", "!", "╠",
			"═", "╩", "╦", "╬"};

ssize_t init_queue(struct queue *q){
	q->data = malloc(q->capacity * sizeof(size_t));
	if(!q->data)
		return -1;
	q->front = q->data;
	q->end = q->data;
	q->data_end = q->data + q->capacity;
	return 0;
}

void push(struct queue *q, size_t what){
	*q->end++ = what;
	if(q->end == q->data_end)
		q->end = q->data;
}

ssize_t pop(struct queue *q){
	size_t value = *q->front++;
	if(q->front == q->data_end)
		q->front = q->data;
	return value;
}

void free_queue(struct queue *q){
	free(q->data);
}

uint32_t bsr(uint32_t n){
	uint32_t result = 0;
	for(int i = 4; i >= 0; i--){
		if(n >= 1u << (1 << i)){
			result += (1 << i);
			n >>= (1 << i);
		}
	}
	return result;

}

#define max_bits (sizeof(size_t) * 8)
#define max_iterations (max_bits >= 64 ? ((1ull << 32) - 1 - 3) : (1ull << (max_bits / 2)) - 1 - 3)

ssize_t allocate_internals(struct pattern *p){
	if(p->iterations >= max_iterations)
		return -1;
	p->width = 1 + (~1 & (p->iterations + 1));
	p->height = 3 + (~1 & p->iterations);
	p->data = calloc((size_t)p->height * p->width, 1);
	if(!p->data)
		return -1;
	size_t log = bsr(p->iterations + 3); // useful?
	p->endps.capacity = ((size_t)p->iterations + 3) * log * log; //(p->iterations + 1) * 0.7 ? no idea
	if(init_queue(&p->endps)){
		free(p->data);
		return -1;
	}
	return 0;
}

void init_pattern(struct pattern *p){
	while(allocate_internals(p)){
		fprintf(stderr, "Couldn't allocate the field for %u iterations, retrying for %u\n", p->iterations, p->iterations >> 1);
		p->iterations >>= 1;
	}
	// memset(p->data, 0, (size_t)p->width * p->height); Assuming the memory is already zeroed
	size_t pos = (size_t)p->width * (p->height / 2) + p->width / 2;
	p->data[pos] = 0b0011; // right, left, down, up
	p->data[pos - p->width] = 0b0010; // up
	p->data[pos + p->width] = 0b0001;
	push(&p->endps, pos + p->width);
	push(&p->endps, pos - p->width);
}


void free_pattern(struct pattern *p){
	free(p->data);
	free_queue(&p->endps);
}

void perform_iterations(struct pattern *p){
	struct queue *endpoints = &p->endps;
	size_t last_pixel_id = (size_t)p->width * p->height - 1;
	size_t last_row_id = (size_t)p->width * (p->height - 1);
	for(size_t i = 0; i < p->iterations; i++){
		size_t *cend = endpoints->end;
		if(p->data[*endpoints->front] & 0b0011){ // manual unswitching; do we need that?
			while(endpoints->front != cend){
				size_t pos = pop(endpoints);
				uint_fast8_t current = p->data[pos];
				if(current & (current - 1)) // popcnt(current) != 1
					continue;
				p->data[pos] |= 0b1100;
				if(!p->data[pos + 1])
					push(endpoints, pos + 1);
				p->data[pos + 1] |= 0b0100;
				if(!p->data[pos - 1])
					push(endpoints, pos - 1);
				p->data[pos - 1] |= 0b1000;
			}
		}
		else{
			while(endpoints->front != cend){
				size_t pos = pop(endpoints);
				uint_fast8_t current = p->data[pos];
				if(current & (current - 1))
					continue;
				p->data[pos] |= 0b0011;
				if(!p->data[pos + p->width])
					push(endpoints, pos + p->width);
				p->data[pos + p->width] |= 0b0001;
				if(!p->data[pos - p->width])
					push(endpoints, pos - p->width);
				p->data[pos - p->width] |= 0b0010;
			}
		}
	}
}

void print_pattern(struct pattern *p){
	for(size_t i = 0; i < p->height; i++){
		for(uint32_t j = 0; j < p->width; j++)
			fputs(symbols[p->data[i * p->width + j]], stdout);
		putc('\n', stdout);
	}
}

void print_pattern_hvscaled(struct pattern *p, uint32_t h_length, uint32_t v_length){

	char *row_base = calloc(p->width * (h_length + 1) + 1, 3), *row = row_base; // buffer for the row
	char *filler_row_base = calloc(p->width * (h_length + 1) + 1, 3), *filler_row = filler_row_base; // buffer for rows used for vertical scaling
	for(size_t i = 0; i < p->height; i++){
		for(uint32_t j = 0; j < p->width; j++){
			uint_fast8_t current = p->data[i * p->width + j];
			if(current){
				*((uint32_t *)row) = *(uint32_t *)(symbols[current]); // the box chars we use are just 3 bytes long in UTF-8
				row += 3;
			}
			else
				*row++ = ' ';
			if(current & 0b0010){	
				*((uint32_t *)filler_row) = *(uint32_t *)(symbols[0b0011]);
				filler_row += 3;
			}
			else
				*filler_row++ = ' ';
			if(j + 1 < p->width){
				if(current & 0b1000)
					for(uint32_t k = 0; k < h_length; k++, row += 3)
						*(uint32_t *)row = *(uint32_t *)(symbols[0b1100]);
				else
					for(uint32_t k = 0; k < h_length; k++)
						*row++ = ' ';
				for(uint32_t k = 0; k < h_length; k++)
					*filler_row++ = ' ';
			}
		}
		*row = '\0';
		*filler_row = '\0';
		row = row_base;
		filler_row = filler_row_base;
		puts(row);
		if(i + 1 < p->height)
			for(int j = 0; j < v_length; j++)
				puts(filler_row);
	}
}

int main(int argc, char *argv[]){
	struct pattern p;
#ifdef __WIN64__
    srand(time(0));
#else
    srand(clock());
#endif
	if(argc <= 1){
		p.iterations = rand() % 16;
		fprintf(stderr, "No arguments provided, assuming a random iterations number of %d\n", p.iterations);
	}
	else if(argc == 2){
		if(!isdigit(argv[1][0])){
			fprintf(stderr, "Provided argument for the iterations number doesn't start with a digit\n");
			return -1;
		}
		p.iterations = atoi(argv[1]);

	}
	else{
		fprintf(stderr, "Too many arguments. Please enter the number of iterations for the pattern creation algorithm\n");
		return -1;
	}
	init_pattern(&p);
	perform_iterations(&p);
	print_pattern_hvscaled(&p, 3, 1);
	free_pattern(&p);
	return 0;
}

