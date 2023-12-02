#ifndef COREMAP_H
#define COREMAP_H

class Coremap {
  public:
    Coremap(int numPages);
    ~Coremap();
    int find_space(int vpn);
    int find_page_replace(int vpn, int* swaped);
  
  private:
    int* coremap;
    int numPages;
    int fifo_to_replace;
};

#endif // COREMAP_H
