#include "../include/bpt.h"

#include <stdlib.h>

#include <list>
#include <algorithm>
using std::swap;
using std::binary_search;
using std::lower_bound;
using std::upper_bound;

namespace bpt {

/* custom compare operator for STL algorithms */
OPERATOR_KEYCMP(index_t)
OPERATOR_KEYCMP(record_t)

/* helper iterating function */
template<class T>
inline typename T::child_t begin(T &node) {
    return node.children; 
}
template<class T>
inline typename T::child_t end(T &node) {
    return node.children + node.n;
}

/* helper searching function */
inline index_t *find(internal_node_t &node, const key_t &key) {
    return upper_bound(begin(node), end(node) - 1, key);
}
inline record_t *find(leaf_node_t &node, const key_t &key) {
    return lower_bound(begin(node), end(node), key);
}

bplus_tree::bplus_tree(const char *p, bool force_empty)
    : fp(NULL), fp_level(0)
{
    bzero(path, sizeof(path));
    strcpy(path, p);

    if (!force_empty)
        // read tree from file
        if (map(&meta, OFFSET_META) != 0)
            force_empty = true;

    if (force_empty) {
        open_file("w+"); // truncate file

        // create empty tree if file doesn't exist
        init_from_empty();
        close_file();
    }
}

int bplus_tree::search(const key_t& key, value_t *value) const
{
    leaf_node_t leaf;
    map(&leaf, search_leaf(key));

    // finding the record
    record_t *record = find(leaf, key);
    if (record != leaf.children + leaf.n) {
        // always return the lower bound
        *value = record->value;

        return keycmp(record->key, key);
    } else {
        return -1;
    }
}

int bplus_tree::search_range(key_t *left, const key_t &right,
                             value_t *values, size_t max, bool *next) const
{
    if (left == NULL || keycmp(*left, right) > 0)
        return -1;

    off_t off_left = search_leaf(*left);
    off_t off_right = search_leaf(right);
    off_t off = off_left;
    size_t i = 0;
    record_t *b, *e;

    leaf_node_t leaf;
    while (off != off_right && off != 0 && i < max) {
        map(&leaf, off);

        // start point
        if (off_left == off) 
            b = find(leaf, *left);
        else
            b = begin(leaf);

        // copy
        e = leaf.children + leaf.n;
        for (; b != e && i < max; ++b, ++i)
            values[i] = b->value;

        off = leaf.next;
    }

    // the last leaf
    if (i < max) {
        map(&leaf, off_right);

        b = find(leaf, *left);
        e = upper_bound(begin(leaf), end(leaf), right);
        for (; b != e && i < max; ++b, ++i)
            values[i] = b->value;
    }

    // mark for next iteration
    if (next != NULL) {
        if (i == max && b != e) {
            *next = true;
            *left = b->key;
        } else {
            *next = false;
        }
    }

    return i;
}

int bplus_tree::remove(const key_t& key)
{
    internal_node_t parent;
    leaf_node_t leaf;

    // find parent node
    off_t parent_off = search_index(key);
    map(&parent, parent_off);

    // find current node
    index_t *where = find(parent, key);
    off_t offset = where->child;
    map(&leaf, offset);

    // verify
    if (!binary_search(begin(leaf), end(leaf), key))
        return -1;

    size_t min_n = meta.leaf_node_num == 1 ? 0 : meta.order / 2;
    assert(leaf.n >= min_n && leaf.n <= meta.order);

    // delete the key
    record_t *to_delete = find(leaf, key);
    std::copy(to_delete + 1, end(leaf), to_delete);
    leaf.n--;

    // merge or borrow
    if (leaf.n < min_n) {
        // first borrow from left
        bool borrowed = false;
        if (leaf.prev != 0)
            borrowed = borrow_key(false, leaf);

        // then borrow from right
        if (!borrowed && leaf.next != 0)
            borrowed = borrow_key(true, leaf);

        // finally we merge
        if (!borrowed) {
            assert(leaf.next != 0 || leaf.prev != 0);

            key_t index_key;

            if (where == end(parent) - 1) {
                // if leaf is last element then merge | prev | leaf |
                assert(leaf.prev != 0);
                leaf_node_t prev;
                map(&prev, leaf.prev);
                index_key = begin(prev)->key;

                merge_leafs(&prev, &leaf);
                node_remove(&prev, &leaf);
                unmap(&prev, leaf.prev);
            } else {
                // else merge | leaf | next |
                assert(leaf.next != 0);
                leaf_node_t next;
                map(&next, leaf.next);
                index_key = begin(leaf)->key;

                merge_leafs(&leaf, &next);
                node_remove(&leaf, &next);
                unmap(&leaf, offset);
            }

            // remove parent's key
            remove_from_index(parent_off, parent, index_key);
        } else {
            unmap(&leaf, offset);
        }
    } else {
        unmap(&leaf, offset);
    }

    return 0;
}

int bplus_tree::insert(const key_t& key, value_t value)
{
    off_t parent = search_index(key);
    off_t offset = search_leaf(parent, key);
    leaf_node_t leaf;
    map(&leaf, offset);

    // check if we have the same key
    if (binary_search(begin(leaf), end(leaf), key))
        return 1;

    if (leaf.n == meta.order) {
        // split when full

        // new sibling leaf
        leaf_node_t new_leaf;
        node_create(offset, &leaf, &new_leaf);

        // find even split point
        size_t point = leaf.n / 2;
        bool place_right = keycmp(key, leaf.children[point].key) > 0;
        if (place_right)
            ++point;

        // split
        std::copy(leaf.children + point, leaf.children + leaf.n,
                  new_leaf.children);
        new_leaf.n = leaf.n - point;
        leaf.n = point;

        // which part do we put the key
        if (place_right)
            insert_record_no_split(&new_leaf, key, value);
        else
            insert_record_no_split(&leaf, key, value);

        // save leafs
        unmap(&leaf, offset);
        unmap(&new_leaf, leaf.next);

        // insert new index key
        insert_key_to_index(parent, new_leaf.children[0].key,
                            offset, leaf.next);
    } else {
        insert_record_no_split(&leaf, key, value);
        unmap(&leaf, offset);
    }

    return 0;
}

int bplus_tree::update(const key_t& key, value_t value)
{
    off_t offset = search_leaf(key);
    leaf_node_t leaf;
    map(&leaf, offset);

    record_t *record = find(leaf, key);
    if (record != leaf.children + leaf.n)
        if (keycmp(key, record->key) == 0) {
            record->value = value;
            unmap(&leaf, offset);

            return 0;
        } else {
            return 1;
        }
    else
        return -1;
}

void bplus_tree::remove_from_index(off_t offset, internal_node_t &node,
                                   const key_t &key)
{
    size_t min_n = meta.root_offset == offset ? 1 : meta.order / 2;
    assert(node.n >= min_n && node.n <= meta.order);

    // remove key
    key_t index_key = begin(node)->key;
    index_t *to_delete = find(node, key);
    if (to_delete != end(node)) {
        (to_delete + 1)->child = to_delete->child;
        std::copy(to_delete + 1, end(node), to_delete);
    }
    node.n--;

    // remove to only one key
    if (node.n == 1 && meta.root_offset == offset &&
                       meta.internal_node_num != 1)
    {
        unalloc(&node, meta.root_offset);
        meta.height--;
        meta.root_offset = node.children[0].child;
        unmap(&meta, OFFSET_META);
        return;
    }

    // merge or borrow
    if (node.n < min_n) {
        internal_node_t parent;
        map(&parent, node.parent);

        // first borrow from left
        bool borrowed = false;
        if (offset != begin(parent)->child)
            borrowed = borrow_key(false, node, offset);

        // then borrow from right
        if (!borrowed && offset != (end(parent) - 1)->child)
            borrowed = borrow_key(true, node, offset);

        // finally we merge
        if (!borrowed) {
            assert(node.next != 0 || node.prev != 0);

            if (offset == (end(parent) - 1)->child) {
                // if leaf is last element then merge | prev | leaf |
                assert(node.prev != 0);
                internal_node_t prev;
                map(&prev, node.prev);

                // merge
                index_t *where = find(parent, begin(prev)->key);
                reset_index_children_parent(begin(node), end(node), node.prev);
                merge_keys(where, prev, node);
                unmap(&prev, node.prev);
            } else {
                // else merge | leaf | next |
                assert(node.next != 0);
                internal_node_t next;
                map(&next, node.next);

                // merge
                index_t *where = find(parent, index_key);
                reset_index_children_parent(begin(next), end(next), offset);
                merge_keys(where, node, next);
                unmap(&node, offset);
            }

            // remove parent's key
            remove_from_index(node.parent, parent, index_key);
        } else {
            unmap(&node, offset);
        }
    } else {
        unmap(&node, offset);
    }
}

bool bplus_tree::borrow_key(bool from_right, internal_node_t &borrower,
                            off_t offset)
{
    typedef typename internal_node_t::child_t child_t;

    off_t lender_off = from_right ? borrower.next : borrower.prev;
    internal_node_t lender;
    map(&lender, lender_off);

    assert(lender.n >= meta.order / 2);
    if (lender.n != meta.order / 2) {
        child_t where_to_lend, where_to_put;

        internal_node_t parent;

        // swap keys, draw on paper to see why
        if (from_right) {
            where_to_lend = begin(lender);
            where_to_put = end(borrower);

            map(&parent, borrower.parent);
            child_t where = lower_bound(begin(parent), end(parent) - 1,
                                        (end(borrower) -1)->key);
            where->key = where_to_lend->key;
            unmap(&parent, borrower.parent);
        } else {
            where_to_lend = end(lender) - 1;
            where_to_put = begin(borrower);

            map(&parent, lender.parent);
            child_t where = find(parent, begin(lender)->key);
            where_to_put->key = where->key;
            where->key = (where_to_lend - 1)->key;
            unmap(&parent, lender.parent);
        }

        // store
        std::copy_backward(where_to_put, end(borrower), end(borrower) + 1);
        *where_to_put = *where_to_lend;
        borrower.n++;

        // erase
        reset_index_children_parent(where_to_lend, where_to_lend + 1, offset);
        std::copy(where_to_lend + 1, end(lender), where_to_lend);
        lender.n--;
        unmap(&lender, lender_off);
        return true;
    }

    return false;
}

bool bplus_tree::borrow_key(bool from_right, leaf_node_t &borrower)
{
    off_t lender_off = from_right ? borrower.next : borrower.prev;
    leaf_node_t lender;
    map(&lender, lender_off);

    assert(lender.n >= meta.order / 2);
    if (lender.n != meta.order / 2) {
        typename leaf_node_t::child_t where_to_lend, where_to_put;

        // decide offset and update parent's index key
        if (from_right) {
            where_to_lend = begin(lender);
            where_to_put = end(borrower);
            change_parent_child(borrower.parent, begin(borrower)->key,
                                lender.children[1].key);
        } else {
            where_to_lend = end(lender) - 1;
            where_to_put = begin(borrower);
            change_parent_child(lender.parent, begin(lender)->key,
                                where_to_lend->key);
        }

        // store
        std::copy_backward(where_to_put, end(borrower), end(borrower) + 1);
        *where_to_put = *where_to_lend;
        borrower.n++;

        // erase
        std::copy(where_to_lend + 1, end(lender), where_to_lend);
        lender.n--;
        unmap(&lender, lender_off);
        return true;
    }

    return false;
}

void bplus_tree::change_parent_child(off_t parent, const key_t &o,
                                     const key_t &n)
{
    internal_node_t node;
    map(&node, parent);

    index_t *w = find(node, o);
    assert(w != node.children + node.n); 

    w->key = n;
    unmap(&node, parent);
    if (w == node.children + node.n - 1) {
        change_parent_child(node.parent, o, n);
    }
}

void bplus_tree::merge_leafs(leaf_node_t *left, leaf_node_t *right)
{
    std::copy(begin(*right), end(*right), end(*left));
    left->n += right->n;
}

void bplus_tree::merge_keys(index_t *where,
                            internal_node_t &node, internal_node_t &next)
{
    //(end(node) - 1)->key = where->key;
    //where->key = (end(next) - 1)->key;
    std::copy(begin(next), end(next), end(node));
    node.n += next.n;
    node_remove(&node, &next);
}

void bplus_tree::insert_record_no_split(leaf_node_t *leaf,
                                        const key_t &key, const value_t &value)
{
    record_t *where = upper_bound(begin(*leaf), end(*leaf), key);
    std::copy_backward(where, end(*leaf), end(*leaf) + 1);

    where->key = key;
    where->value = value;
    leaf->n++;
}

void bplus_tree::insert_key_to_index(off_t offset, const key_t &key,
                                     off_t old, off_t after)
{
    if (offset == 0) {
        // create new root node
        internal_node_t root;
        root.next = root.prev = root.parent = 0;
        meta.root_offset = alloc(&root);
        meta.height++;

        // insert `old` and `after`
        root.n = 2;
        root.children[0].key = key;
        root.children[0].child = old;
        root.children[1].child = after;

        unmap(&meta, OFFSET_META);
        unmap(&root, meta.root_offset);

        // update children's parent
        reset_index_children_parent(begin(root), end(root),
                                    meta.root_offset);
        return;
    }

    internal_node_t node;
    map(&node, offset);
    assert(node.n <= meta.order);

    if (node.n == meta.order) {
        // split when full

        internal_node_t new_node;
        node_create(offset, &node, &new_node);

        // find even split point
        size_t point = (node.n - 1) / 2;
        bool place_right = keycmp(key, node.children[point].key) > 0;
        if (place_right)
            ++point;

        // prevent the `key` being the right `middle_key`
        // example: insert 48 into |42|45| 6|  |
        if (place_right && keycmp(key, node.children[point].key) < 0)
            point--;

        key_t middle_key = node.children[point].key;

        // split
        std::copy(begin(node) + point + 1, end(node), begin(new_node));
        new_node.n = node.n - point - 1;
        node.n = point + 1;

        // put the new key
        if (place_right)
            insert_key_to_index_no_split(new_node, key, after);
        else
            insert_key_to_index_no_split(node, key, after);

        unmap(&node, offset);
        unmap(&new_node, node.next);

        // update children's parent
        reset_index_children_parent(begin(new_node), end(new_node), node.next);

        // give the middle key to the parent
        // note: middle key's child is reserved
        insert_key_to_index(node.parent, middle_key, offset, node.next);
    } else {
        insert_key_to_index_no_split(node, key, after);
        unmap(&node, offset);
    }
}

void bplus_tree::insert_key_to_index_no_split(internal_node_t &node,
                                              const key_t &key, off_t value)
{
    index_t *where = upper_bound(begin(node), end(node) - 1, key);

    // move later index forward
    std::copy_backward(where, end(node), end(node) + 1);

    // insert this key
    where->key = key;
    where->child = (where + 1)->child;
    (where + 1)->child = value;

    node.n++;
}

void bplus_tree::reset_index_children_parent(index_t *begin, index_t *end,
                                             off_t parent)
{
    // this function can change both internal_node_t and leaf_node_t's parent
    // field, but we should ensure that:
    // 1. sizeof(internal_node_t) <= sizeof(leaf_node_t)
    // 2. parent field is placed in the beginning and have same size
    internal_node_t node;
    while (begin != end) {
        map(&node, begin->child);
        node.parent = parent;
        unmap(&node, begin->child, SIZE_NO_CHILDREN);
        ++begin;
    }
}

off_t bplus_tree::search_index(const key_t &key) const
{
    off_t org = meta.root_offset;
    int height = meta.height;
    while (height > 1) {
        internal_node_t node;
        map(&node, org);

        index_t *i = upper_bound(begin(node), end(node) - 1, key);
        org = i->child;
        --height;
    }

    return org;
}

off_t bplus_tree::search_leaf(off_t index, const key_t &key) const
{
    internal_node_t node;
    map(&node, index);

    index_t *i = upper_bound(begin(node), end(node) - 1, key);
    return i->child;
}

template<class T>
void bplus_tree::node_create(off_t offset, T *node, T *next)
{
    // new sibling node
    next->parent = node->parent;
    next->next = node->next;
    next->prev = offset;
    node->next = alloc(next);
    // update next node's prev
    if (next->next != 0) {
        T old_next;
        map(&old_next, next->next, SIZE_NO_CHILDREN);
        old_next.prev = node->next;
        unmap(&old_next, next->next, SIZE_NO_CHILDREN);
    }
    unmap(&meta, OFFSET_META);
}

template<class T>
void bplus_tree::node_remove(T *prev, T *node)
{
    unalloc(node, prev->next);
    prev->next = node->next;
    if (node->next != 0) {
        T next;
        map(&next, node->next, SIZE_NO_CHILDREN);
        next.prev = node->prev;
        unmap(&next, node->next, SIZE_NO_CHILDREN);
    }
    unmap(&meta, OFFSET_META);
}

void bplus_tree::init_from_empty()
{
    // init default meta
    bzero(&meta, sizeof(meta_t));
    meta.order = BP_ORDER;
    meta.value_size = sizeof(value_t);
    meta.key_size = sizeof(key_t);
    meta.height = 1;
    meta.slot = OFFSET_BLOCK;

    // init root node
    internal_node_t root;
    root.next = root.prev = root.parent = 0;
    meta.root_offset = alloc(&root);

    // init empty leaf
    leaf_node_t leaf;
    leaf.next = leaf.prev = 0;
    leaf.parent = meta.root_offset;
    meta.leaf_offset = root.children[0].child = alloc(&leaf);

    // save
    unmap(&meta, OFFSET_META);
    unmap(&root, meta.root_offset);
    unmap(&leaf, root.children[0].child);
}

}
