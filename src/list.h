#ifndef LIST_H
#define LIST_H

#define LIST_INIT_HEAD(head) \
    (head) = (struct list){ &(head), &(head) }

#define LIST_HEAD(head) ((head)->next == (head) ? NULL : (head)->next)
#define LIST_TAIL(head) ((head)->prev == (head) ? NULL : (head)->prev)

#define list_for_each(p, head) \
    for ((p) = (head)->next; (p) != (head); (p) = (p)->next)

#define list_for_each_safe(p, n, head) \
    for ((p) = (head)->next, (n) = (p)->next; (p) != (head); (p) = (n), (n) = (p)->next)

#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

struct list
{
    struct list *prev, *next;
};

#define list_add_entry(head, ptr, field) \
    list_add((head), &((ptr)->field))
#define list_add_entry_tail(head, ptr, field) \
    list_add_tail((head), &((ptr)->field))

#define list_add(head, node) \
    _list_add((node), (head), (head)->next)
#define list_add_tail(head, node) \
    _list_add((node), (head)->prev, (head))

static inline void _list_add(struct list *node, struct list *prev, struct list *next)
{
    next->prev = node;
    node->next = next;
    node->prev = prev;
    prev->next = node;
}

#define list_del_entry(ptr, field) \
    list_del(&((ptr)->field))

#define list_del(node) \
    do { \
        _list_del((node)->prev, (node)->next); \
        (node)->prev = (node)->next = NULL; \
    } while (0)

static inline void _list_del(struct list *prev, struct list *next)
{
    next->prev = prev;
    prev->next = next;
}

#endif
