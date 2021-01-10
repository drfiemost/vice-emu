
#ifndef CARTCONV_H_
#define CARTCONV_H_

typedef struct cart_s {
    unsigned char exrom;
    unsigned char game;
    unsigned int sizes;
    unsigned int bank_size;
    unsigned int load_address;
    unsigned char banks;   /* 0 means the amount of banks need to be taken from the load-size and bank-size */
    unsigned int data_type;
    char *name;
    char *opt;
    void (*save)(unsigned int p1, unsigned int p2, unsigned int p3, unsigned int p4, unsigned char gameline, unsigned char exromline);
} cart_t;

extern void cleanup(void);
extern void bin2crt_ok(void);
extern void save_regular_crt(unsigned int p1, unsigned int p2, unsigned int p3, unsigned int p4, unsigned char game, unsigned char exrom);
extern int load_input_file(char *filename);

#endif
