/*
 * Rectangle packing library.
 * v1.1.3
 *
 * Copyright (c) 2017-2021 Daniel Plakhotich
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgement in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */


#ifndef DP_RECT_PACK_H
#define DP_RECT_PACK_H

#include <cassert>
#include <cstddef>
#include <vector>


#define DP_RECT_PACK_VERSION_MAJOR 1
#define DP_RECT_PACK_VERSION_MINOR 1
#define DP_RECT_PACK_VERSION_PATCH 3


namespace dp {
namespace rect_pack {


/**
 * Status of the RectPacker::InsertResult.
 *
 * Only InsertStatus::ok indicates a successful insertion;
 * all other values are kinds of errors.
 */
struct InsertStatus {
    enum Type {
        ok,  ///< Successful insertion
        negativeSize,  ///< Width and/or height is negative
        zeroSize,  ///< Width and/or height is zero

        /**
         * Rectangle is too big to fit in a single page.
         *
         * Width and/or height of the rectangle exceeds the maximum
         * size a single page can hold, which is the maximum page size
         * minus the padding.
         *
         * \sa RectPacker::RectPacker()
         */
        rectTooBig
    };
};


// A note on the implementation.
// The current algorithm is absolutely the same as in version 1.0.0,
// except that we only keep the leaf nodes of the binary tree. This
// dramatically improves performance and reduces memory usage, but
// growDown() and growRight() methods are harder to understand
// because the leafs insertion order depends on several layers of
// parent branches that don't physically exist. I added comments to
// help you visualize what happens, but it will probably be easier
// to just look at the code of the version 1.0.0.


/**
 * Rectangle packer.
 *
 * GeomT is not required to hold negative numbers, and thus can be
 * an unsigned integer. It's also possible to use a floating-point
 * or a custom numeric type.
 *
 * A custom type for GeomT should support:
 *     * Implicit construction from an integer >= 0
 *     * Addition and subtraction (including compound assignment)
 *     * Comparison
 *
 * \tparam GeomT numeric type to use for geometry
 */
template<typename GeomT = int>
class RectPacker {
public:
    struct Spacing {
        GeomT x;  ///< Horizontal spacing
        GeomT y;  ///< Vertical spacing

        /**
         * Construct Spacing with the same spacing for both dimensions.
         */
        explicit Spacing(GeomT spacing)
            : x(spacing)
            , y(spacing)
        {}

        Spacing(GeomT x, GeomT y)
            : x(x)
            , y(y)
        {}
    };

    struct Padding {
        GeomT top;
        GeomT bottom;
        GeomT left;
        GeomT right;

        /**
         * Construct Padding with the same padding for all sides.
         */
        explicit Padding(GeomT padding)
            : top(padding)
            , bottom(padding)
            , left(padding)
            , right(padding)
        {}

        Padding(GeomT top, GeomT bottom, GeomT left, GeomT right)
            : top(top)
            , bottom(bottom)
            , left(left)
            , right(right)
        {}
    };

    struct Position {
        GeomT x;
        GeomT y;

        Position()
            : x()
            , y()
        {}

        Position(GeomT x, GeomT y)
            : x(x)
            , y(y)
        {}
    };

    /**
     * Result returned by RectPacker::insert().
     */
    struct InsertResult {
        /**
         * Status of the insertion.
         *
         * \warning If InsertResult.status is not InsertStatus::ok,
         *     values of all other fields of InsertResult are undefined.
         */
        InsertStatus::Type status;

        /**
         * Position of the inserted rectangle within the page.
         */
        Position pos;

        /**
         * Index of the page in which the rectangle was inserted.
         *
         * \sa getPageSize()
         */
        std::size_t pageIndex;
    };

    /**
     * RectPacker constructor.
     *
     * maxPageWidth and maxPageHeight define the maximum size of
     * a single page, including the padding. Depending on this limit
     * and the features of GeomT, a RectPacker can work in multipage
     * or infinite single-page mode.
     *
     * To enable infinite single-page mode, you have two choices,
     * depending on the properties of GeomT:
     *     * If GeomT has a physical limit (like any standard integer),
     *       you can set the maximum size to the maximum positive
     *       value GeomT can hold.
     *     * Otherwise, if GeomT is a floating-point type or a custom
     *       unbounded type, you can set the maximum size to a huge
     *       value or, if supported by the type, a magic value that
     *       always bigger than any finite number (like a positive
     *       infinity for floating-point types).
     *
     * If GeomT can hold negative values, the maximum page size, spacing,
     * and padding will be clamped to 0. Keep in mind that if the
     * maximum page size is 0, or if the total padding greater or equal
     * to the maximum page size, pages will have no free space for
     * rectangles, and all calls to insert() will result in
     * InsertStatus::rectTooBig.
     *
     * \param maxPageWidth maximum width of a page, including
     *     the horizontal padding
     * \param maxPageHeight maximum height of a page, including
     *     the vertical padding
     * \param rectsSpacing space between rectangles
     * \param pagePadding space between rectangles and edges of a page
     */
    RectPacker(
        GeomT maxPageWidth, GeomT maxPageHeight,
        const Spacing& rectsSpacing = Spacing(0),
        const Padding& pagePadding = Padding(0))
            : ctx(maxPageWidth, maxPageHeight, rectsSpacing, pagePadding)
            , pages(1)
    {}

    /**
     * Return the current number of pages.
     *
     * \returns number of pages (always > 0)
     */
    std::size_t getNumPages() const
    {
        return pages.size();
    }

    /**
     * Return the current size of the page.
     *
     * \param pageIndex index of the page in range [0..getNumPages())
     * \param[out] width width of the page
     * \param[out] height height of the page
     *
     * \sa getNumPages(), InsertResult::pageIndex
     */
    void getPageSize(std::size_t pageIndex, GeomT& width, GeomT& height) const
    {
        const Size size = pages[pageIndex].getSize(ctx);
        width = size.w;
        height = size.h;
    }

    /**
     * Insert a rectangle.
     *
     * The rectangles you'll feed to insert() should be sorted in
     * descending order by comparing first by height, then by width.
     * A comparison function for std::sort may look like the following:
     * \code
     *     bool compare(const T& a, const T& b)
     *     {
     *         if (a.height != b.height)
     *             return a.height > b.height;
     *         else
     *             return a.width > b.width;
     *     }
     * \endcode
     *
     * \param width width of the rectangle
     * \param height height of the rectangle
     * \returns InsertResult
     */
    InsertResult insert(GeomT width, GeomT height);
private:
    struct Size {
        GeomT w;
        GeomT h;

        Size(GeomT w, GeomT h)
            : w(w)
            , h(h)
        {}
    };

    struct Context;
    class Page {
    public:
        Page()
            : nodes()
            , rootSize(0, 0)
            , growDownRootBottomIdx(0)
        {}

        Size getSize(const Context& ctx) const
        {
            return Size(
                ctx.padding.left + rootSize.w + ctx.padding.right,
                ctx.padding.top + rootSize.h + ctx.padding.bottom);
        }

        bool insert(Context& ctx, const Size& rect, Position& pos);
    private:
        struct Node {
            Position pos;
            Size size;

            Node(GeomT x, GeomT y, GeomT w, GeomT h)
                : pos(x, y)
                , size(w, h)
            {}
        };

        // Leaf nodes of the binary tree in depth-first order
        std::vector<Node> nodes;
        Size rootSize;
        // The index of the first leaf bottom node of the new root
        // created in growDown(). See the method for more details.
        std::size_t growDownRootBottomIdx;

        bool tryInsert(Context& ctx, const Size& rect, Position& pos);
        bool findNode(
            const Size& rect,
            std::size_t& nodeIdx, Position& pos) const;
        void subdivideNode(
            Context& ctx, std::size_t nodeIdx, const Size& rect);
        bool tryGrow(Context& ctx, const Size& rect, Position& pos);
        void growDown(Context& ctx, const Size& rect, Position& pos);
        void growRight(Context& ctx, const Size& rect, Position& pos);
    };

    struct Context {
        Size maxSize;
        Spacing spacing;
        Padding padding;

        Context(
            GeomT maxPageWidth, GeomT maxPageHeight,
            const Spacing& rectsSpacing, const Padding& pagePadding);

        static void subtractPadding(GeomT& padding, GeomT& size);
    };

    Context ctx;
    std::vector<Page> pages;
};


template<typename GeomT>
typename RectPacker<GeomT>::InsertResult
RectPacker<GeomT>::insert(GeomT width, GeomT height)
{
    InsertResult result;

    if (width < 0 || height < 0) {
        result.status = InsertStatus::negativeSize;
        return result;
    }

    if (width == 0 || height == 0) {
        result.status = InsertStatus::zeroSize;
        return result;
    }

    if (width > ctx.maxSize.w || height > ctx.maxSize.h) {
        result.status = InsertStatus::rectTooBig;
        return result;
    }

    const Size rect(width, height);

    for (std::size_t i = 0; i < pages.size(); ++i)
        if (pages[i].insert(ctx, rect, result.pos)) {
            result.status = InsertStatus::ok;
            result.pageIndex = i;
            return result;
        }

    pages.push_back(Page());
    Page& page = pages.back();
    page.insert(ctx, rect, result.pos);
    result.status = InsertStatus::ok;
    result.pageIndex = pages.size() - 1;

    return result;
}


template<typename GeomT>
bool RectPacker<GeomT>::Page::insert(
    Context& ctx, const Size& rect, Position& pos)
{
    assert(rect.w > 0);
    assert(rect.w <= ctx.maxSize.w);
    assert(rect.h > 0);
    assert(rect.h <= ctx.maxSize.h);

    // The first insertion should be handled especially since
    // growRight() and growDown() add spacing between the root
    // and the inserted rectangle.
    if (rootSize.w == 0) {
        rootSize = rect;
        pos.x = ctx.padding.left;
        pos.y = ctx.padding.top;

        return true;
    }

    return tryInsert(ctx, rect, pos) || tryGrow(ctx, rect, pos);
}


template<typename GeomT>
bool RectPacker<GeomT>::Page::tryInsert(
    Context& ctx, const Size& rect, Position& pos)
{
    std::size_t nodeIdx;
    if (findNode(rect, nodeIdx, pos)) {
        subdivideNode(ctx, nodeIdx, rect);
        return true;
    }

    return false;
}


template<typename GeomT>
bool RectPacker<GeomT>::Page::findNode(
    const Size& rect, std::size_t& nodeIdx, Position& pos) const
{
    for (nodeIdx = 0; nodeIdx < nodes.size(); ++nodeIdx) {
        const Node& node = nodes[nodeIdx];
        if (rect.w <= node.size.w && rect.h <= node.size.h) {
            pos = node.pos;
            return true;
        }
    }

    return false;
}


/**
 * Called after a rectangle was inserted in the top left corner of
 * a free node to create child nodes from free space, if any.
 *
 * The node is first cut horizontally along the rect's bottom,
 * then vertically along the right edge of the rect. Splitting
 * that way is crucial for the algorithm to work correctly.
 *
 *      +---+
 *      |   |
 *  +---+---+
 *  |       |
 *  +-------+
 */
template<typename GeomT>
void RectPacker<GeomT>::Page::subdivideNode(
    Context& ctx, std::size_t nodeIdx, const Size& rect)
{
    assert(nodeIdx < nodes.size());

    Node& node = nodes[nodeIdx];

    assert(node.size.w >= rect.w);
    const GeomT rightW = node.size.w - rect.w;
    const bool hasSpaceRight = rightW > ctx.spacing.x;

    assert(node.size.h >= rect.h);
    const GeomT bottomH = node.size.h - rect.h;
    const bool hasSpaceBelow = bottomH > ctx.spacing.y;

    if (hasSpaceRight) {
        // Right node replaces the current

        const GeomT bottomX = node.pos.x;
        const GeomT bottomW = node.size.w;

        node.pos.x += rect.w + ctx.spacing.x;
        node.size.w = rightW - ctx.spacing.x;
        node.size.h = rect.h;

        if (hasSpaceBelow) {
            nodes.insert(
                nodes.begin() + nodeIdx + 1,
                Node(
                    bottomX,
                    node.pos.y + rect.h + ctx.spacing.y,
                    bottomW,
                    bottomH - ctx.spacing.y));

            if (nodeIdx <= growDownRootBottomIdx)
                ++growDownRootBottomIdx;
        }
    } else if (hasSpaceBelow) {
        // Bottom node replaces the current
        node.pos.y += rect.h + ctx.spacing.y;
        node.size.h = bottomH - ctx.spacing.y;
    } else {
        nodes.erase(nodes.begin() + nodeIdx);
        if (nodeIdx < growDownRootBottomIdx)
            --growDownRootBottomIdx;
    }
}


template<typename GeomT>
bool RectPacker<GeomT>::Page::tryGrow(
    Context& ctx, const Size& rect, Position& pos)
{
    assert(ctx.maxSize.w >= rootSize.w);
    const GeomT freeW = ctx.maxSize.w - rootSize.w;
    assert(ctx.maxSize.h >= rootSize.h);
    const GeomT freeH = ctx.maxSize.h - rootSize.h;

    const bool canGrowDown = (
        freeH >= rect.h && freeH - rect.h >= ctx.spacing.y);
    const bool mustGrowDown = (
        canGrowDown
        && freeW >= ctx.spacing.x
        && (rootSize.w + ctx.spacing.x
            >= rootSize.h + rect.h + ctx.spacing.y));
    if (mustGrowDown) {
        growDown(ctx, rect, pos);
        return true;
    }

    const bool canGrowRight = (
        freeW >= rect.w && freeW - rect.w >= ctx.spacing.x);
    if (canGrowRight) {
        growRight(ctx, rect, pos);
        return true;
    }

    if (canGrowDown) {
        growDown(ctx, rect, pos);
        return true;
    }

    return false;
}


template<typename GeomT>
void RectPacker<GeomT>::Page::growDown(
    Context& ctx, const Size& rect, Position& pos)
{
    assert(ctx.maxSize.h > rootSize.h);
    assert(ctx.maxSize.h - rootSize.h >= rect.h);
    assert(ctx.maxSize.h - rootSize.h - rect.h >= ctx.spacing.y);

    pos.x = ctx.padding.left;
    pos.y = ctx.padding.top + rootSize.h + ctx.spacing.y;

    if (rootSize.w < rect.w) {
        if (rect.w - rootSize.w > ctx.spacing.x) {
            // The auxiliary node becomes the right child of the new
            // root. It contains the current root (bottom child) and
            // free space at the current root's right (right child).
            nodes.insert(
                nodes.begin(),
                Node(
                    ctx.padding.left + rootSize.w + ctx.spacing.x,
                    ctx.padding.top,
                    rect.w - rootSize.w - ctx.spacing.x,
                    rootSize.h));
            ++growDownRootBottomIdx;
        }

        rootSize.w = rect.w;
    } else if (rootSize.w - rect.w > ctx.spacing.x) {
        // Free space at the right of the inserted rect becomes the
        // right child of the rect's node, which in turn is the
        // bottom child of the new root.
        nodes.insert(
            nodes.begin() + growDownRootBottomIdx,
            Node(
                pos.x + rect.w + ctx.spacing.x,
                pos.y,
                rootSize.w - rect.w - ctx.spacing.x,
                rect.h));

        // The inserted node is visited before the node from the next
        // growDown() since the current new root will be the right
        // child of the next root.
        ++growDownRootBottomIdx;
    }

    rootSize.h += ctx.spacing.y + rect.h;
}


template<typename GeomT>
void RectPacker<GeomT>::Page::growRight(
    Context& ctx, const Size& rect, Position& pos)
{
    assert(ctx.maxSize.w > rootSize.w);
    assert(ctx.maxSize.w - rootSize.w >= rect.w);
    assert(ctx.maxSize.w - rootSize.w - rect.w >= ctx.spacing.x);

    pos.x = ctx.padding.left + rootSize.w + ctx.spacing.x;
    pos.y = ctx.padding.top;

    if (rootSize.h < rect.h) {
        if (rect.h - rootSize.h > ctx.spacing.y)
            // The auxiliary node becomes the bottom child of the
            // new root. It contains the current root (right child)
            // and free space at the current root's bottom, if any
            // (bottom child).
            nodes.insert(
                nodes.end(),
                Node(
                    ctx.padding.left,
                    ctx.padding.top + rootSize.h + ctx.spacing.y,
                    rootSize.w,
                    rect.h - rootSize.h - ctx.spacing.y));

        rootSize.h = rect.h;
    } else if (rootSize.h - rect.h > ctx.spacing.y) {
        // Free space at the bottom of the inserted rect becomes the
        // bottom child of the rect's node, which in turn is the
        // right child of the new root node.
        nodes.insert(
            nodes.begin(),
            Node(
                pos.x,
                pos.y + rect.h + ctx.spacing.y,
                rect.w,
                rootSize.h - rect.h - ctx.spacing.y));
        ++growDownRootBottomIdx;
    }

    rootSize.w += ctx.spacing.x + rect.w;
}


template<typename GeomT>
RectPacker<GeomT>::Context::Context(
    GeomT maxPageWidth, GeomT maxPageHeight,
    const Spacing& rectsSpacing, const Padding& pagePadding)
        : maxSize(maxPageWidth, maxPageHeight)
        , spacing(rectsSpacing)
        , padding(pagePadding)
{
    if (maxSize.w < 0)
        maxSize.w = 0;
    if (maxSize.h < 0)
        maxSize.h = 0;

    if (spacing.x < 0)
        spacing.x = 0;
    if (spacing.y < 0)
        spacing.y = 0;

    subtractPadding(padding.top, maxSize.h);
    subtractPadding(padding.bottom, maxSize.h);
    subtractPadding(padding.left, maxSize.w);
    subtractPadding(padding.right, maxSize.w);
}


template<typename GeomT>
void RectPacker<GeomT>::Context::subtractPadding(
    GeomT& padding, GeomT& size)
{
    if (padding < 0)
        padding = 0;
    else if (padding < size)
        size -= padding;
    else {
        padding = size;
        size = 0;
    }
}


}  // namespace rect_pack
}  // namespace dp


#endif  // DP_RECT_PACK_H