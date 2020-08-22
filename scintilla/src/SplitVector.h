// Scintilla source code edit control
/** @file SplitVector.h
 ** Main data structure for holding arrays that handle insertions
 ** and deletions efficiently.
 **/
// Copyright 1998-2007 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.
#pragma once

#ifdef NDEBUG
#define ENABLE_SHOW_DEBUG_INFO	0
#else
#define ENABLE_SHOW_DEBUG_INFO	0
#endif
#if ENABLE_SHOW_DEBUG_INFO
#include <cstdio>
#endif

namespace Scintilla {

template <typename T>
class SplitVector {
protected:
	std::vector<T> body;
	T empty;	/// Returned as the result of out-of-bounds access.
	ptrdiff_t lengthBody;
	ptrdiff_t part1Length;
	ptrdiff_t gapLength;	/// invariant: gapLength == body.size() - lengthBody
	ptrdiff_t growSize;

	/// Move the gap to a particular position so that insertion and
	/// deletion at that point will not require much copying and
	/// hence be fast.
	void GapTo(ptrdiff_t position) noexcept {
		if (position != part1Length) {
			if (position < part1Length) {
				// Moving the gap towards start so moving elements towards end
				std::move_backward(
					body.data() + position,
					body.data() + part1Length,
					body.data() + gapLength + part1Length);
			} else {	// position > part1Length
				// Moving the gap towards end so moving elements towards start
				std::move(
					body.data() + part1Length + gapLength,
					body.data() + gapLength + position,
					body.data() + part1Length);
			}
			part1Length = position;
		}
	}

	/// Check that there is room in the buffer for an insertion,
	/// reallocating if more space needed.
	void RoomFor(ptrdiff_t insertionLength) {
		if (gapLength <= insertionLength) {
			while (growSize < static_cast<ptrdiff_t>(body.size() / 6)) {
				growSize *= 2;
			}
			ReAllocate(body.size() + insertionLength + growSize);
		}
	}

	void Init() {
		body.clear();
		body.shrink_to_fit();
		lengthBody = 0;
		part1Length = 0;
		gapLength = 0;
		growSize = 8;
	}

public:
	/// Construct a split buffer.
	SplitVector() noexcept : empty(), lengthBody(0), part1Length(0), gapLength(0), growSize(8) {
	}

	// Deleted so SplitVector objects can not be copied.
	SplitVector(const SplitVector &) = delete;
	SplitVector(SplitVector &&) = delete;
	void operator=(const SplitVector &) = delete;
	void operator=(SplitVector &&) = delete;

	~SplitVector() = default;

	ptrdiff_t GetGrowSize() const noexcept {
		return growSize;
	}

	void SetGrowSize(ptrdiff_t growSize_) noexcept {
		growSize = growSize_;
	}

	/// Reallocate the storage for the buffer to be newSize and
	/// copy existing contents to the new buffer.
	/// Must not be used to decrease the size of the buffer.
	void ReAllocate(ptrdiff_t newSize) {
		if (newSize < 0)
			throw std::runtime_error("SplitVector::ReAllocate: negative size.");

		if (newSize > static_cast<ptrdiff_t>(body.size())) {
#if ENABLE_SHOW_DEBUG_INFO
			printf("before %s(%td, %zu) part1Length=%td, gapLength=%td, lengthBody=%td, growSize=%td\n",
				__func__, newSize, body.size(), part1Length, gapLength, lengthBody, growSize);
			Platform::DebugPrintf("before %s(%td, %zu) part1Length=%td, gapLength=%td, lengthBody=%td, growSize=%td\n",
				__func__, newSize, body.size(), part1Length, gapLength, lengthBody, growSize);
#endif
			// Move the gap to the end
			GapTo(lengthBody);
			gapLength += newSize - static_cast<ptrdiff_t>(body.size());
			// RoomFor implements a growth strategy but so does vector::resize so
			// ensure vector::resize allocates exactly the amount wanted by
			// calling reserve first.
			body.reserve(newSize);
			body.resize(newSize);
#if ENABLE_SHOW_DEBUG_INFO
			printf("after %s(%td, %zu) part1Length=%td, gapLength=%td, lengthBody=%td, growSize=%td\n",
				__func__, newSize, body.size(), part1Length, gapLength, lengthBody, growSize);
			Platform::DebugPrintf("after %s(%td, %zu) part1Length=%td, gapLength=%td, lengthBody=%td, growSize=%td\n",
				__func__, newSize, body.size(), part1Length, gapLength, lengthBody, growSize);
#endif
		}
	}

	/// Retrieve the element at a particular position.
	/// Retrieving positions outside the range of the buffer returns empty or 0.
	const T& ValueAt(ptrdiff_t position) const noexcept {
		if (position < part1Length) {
			if (position < 0) {
				return empty;
			} else {
				return body[position];
			}
		} else {
			if (position >= lengthBody) {
				return empty;
			} else {
				return body[gapLength + position];
			}
		}
	}

	/// Set the element at a particular position.
	/// Setting positions outside the range of the buffer performs no assignment
	/// but asserts in debug builds.
	template <typename ParamType>
	void SetValueAt(ptrdiff_t position, ParamType&& v) noexcept {
		if (position < part1Length) {
			PLATFORM_ASSERT(position >= 0);
			if (position < 0) {
				;
			} else {
				body[position] = std::forward<ParamType>(v);
			}
		} else {
			PLATFORM_ASSERT(position < lengthBody);
			if (position >= lengthBody) {
				;
			} else {
				body[gapLength + position] = std::forward<ParamType>(v);
			}
		}
	}

	/// Retrieve the element at a particular position.
	/// The position must be within bounds or an assertion is triggered.
	const T &operator[](ptrdiff_t position) const noexcept {
		PLATFORM_ASSERT(position >= 0 && position < lengthBody);
		if (position < part1Length) {
			return body[position];
		} else {
			return body[gapLength + position];
		}
	}

	/// Retrieve reference to the element at a particular position.
	/// This, instead of the const variant, can be used to mutate in-place.
	/// The position must be within bounds or an assertion is triggered.
	T &operator[](ptrdiff_t position) noexcept {
		PLATFORM_ASSERT(position >= 0 && position < lengthBody);
		if (position < part1Length) {
			return body[position];
		} else {
			return body[gapLength + position];
		}
	}

	/// Retrieve the length of the buffer.
	ptrdiff_t Length() const noexcept {
		return lengthBody;
	}

	/// Insert a single value into the buffer.
	/// Inserting at positions outside the current range fails.
	void Insert(ptrdiff_t position, T v) {
		PLATFORM_ASSERT((position >= 0) && (position <= lengthBody));
		if ((position < 0) || (position > lengthBody)) {
			return;
		}
		RoomFor(1);
		GapTo(position);
		body[part1Length] = std::move(v);
		lengthBody++;
		part1Length++;
		gapLength--;
	}

	/// Insert a number of elements into the buffer setting their value.
	/// Inserting at positions outside the current range fails.
	void InsertValue(ptrdiff_t position, ptrdiff_t insertLength, T v) {
		PLATFORM_ASSERT((position >= 0) && (position <= lengthBody));
		if (insertLength > 0) {
			if ((position < 0) || (position > lengthBody)) {
				return;
			}
			RoomFor(insertLength);
			GapTo(position);
			std::fill_n(body.data() + part1Length, insertLength, v);
			lengthBody += insertLength;
			part1Length += insertLength;
			gapLength -= insertLength;
		}
	}

	/// Add some new empty elements.
	/// InsertValue is good for value objects but not for unique_ptr objects
	/// since they can only be moved from once.
	/// Callers can write to the returned pointer to transform inputs without copies.
	T *InsertEmpty(ptrdiff_t position, ptrdiff_t insertLength) {
		PLATFORM_ASSERT((position >= 0) && (position <= lengthBody));
		if (insertLength > 0) {
			if ((position < 0) || (position > lengthBody)) {
				return nullptr;
			}
			RoomFor(insertLength);
			GapTo(position);
			for (ptrdiff_t elem = part1Length; elem < part1Length + insertLength; elem++) {
				T emptyOne = {};
				body[elem] = std::move(emptyOne);
			}
			lengthBody += insertLength;
			part1Length += insertLength;
			gapLength -= insertLength;
		}
		return body.data() + position;
	}

	/// Ensure at least length elements allocated,
	/// appending zero valued elements if needed.
	void EnsureLength(ptrdiff_t wantedLength) {
		if (Length() < wantedLength) {
			InsertEmpty(Length(), wantedLength - Length());
		}
	}

	/// Insert text into the buffer from an array.
	void InsertFromArray(ptrdiff_t positionToInsert, const T s[], ptrdiff_t positionFrom, ptrdiff_t insertLength) {
		PLATFORM_ASSERT((positionToInsert >= 0) && (positionToInsert <= lengthBody));
		if (insertLength > 0) {
			if ((positionToInsert < 0) || (positionToInsert > lengthBody)) {
				return;
			}
			RoomFor(insertLength);
			GapTo(positionToInsert);
			std::copy_n(s + positionFrom, insertLength, body.data() + part1Length);
			lengthBody += insertLength;
			part1Length += insertLength;
			gapLength -= insertLength;
		}
	}

	/// Delete one element from the buffer.
	void Delete(ptrdiff_t position) {
		PLATFORM_ASSERT((position >= 0) && (position < lengthBody));
		DeleteRange(position, 1);
	}

	/// Delete a range from the buffer.
	/// Deleting positions outside the current range fails.
	/// Cannot be noexcept as vector::shrink_to_fit may be called and it may throw.
	void DeleteRange(ptrdiff_t position, ptrdiff_t deleteLength) {
		PLATFORM_ASSERT((position >= 0) && (position + deleteLength <= lengthBody));
		if ((position < 0) || ((position + deleteLength) > lengthBody)) {
			return;
		}
		if ((position == 0) && (deleteLength == lengthBody)) {
			// Full deallocation returns storage and is faster
			Init();
		} else if (deleteLength > 0) {
			GapTo(position);
			lengthBody -= deleteLength;
			gapLength += deleteLength;
		}
	}

	/// Delete all the buffer contents.
	void DeleteAll() {
		DeleteRange(0, lengthBody);
	}

	/// Retrieve a range of elements into an array
	void GetRange(T *buffer, ptrdiff_t position, ptrdiff_t retrieveLength) const noexcept {
		// Split into up to 2 ranges, before and after the split then use memcpy on each.
		ptrdiff_t range1Length = 0;
		if (position < part1Length) {
			range1Length = std::min(retrieveLength, part1Length - position);
		}
		const T* data = body.data() + position;
		std::copy_n(data, range1Length, buffer);
		data += range1Length + gapLength;
		const ptrdiff_t range2Length = retrieveLength - range1Length;
		std::copy_n(data, range2Length, buffer + range1Length);
	}

	/// Compact the buffer and return a pointer to the first element.
	/// Also ensures there is an empty element beyond logical end in case its
	/// passed to a function expecting a NUL terminated string.
	const T *BufferPointer() {
		RoomFor(1);
		GapTo(lengthBody);
		T emptyOne = {};
		body[lengthBody] = std::move(emptyOne);
		return body.data();
	}

	/// Return a pointer to a range of elements, first rearranging the buffer if
	/// needed to make that range contiguous.
	const T *RangePointer(ptrdiff_t position, ptrdiff_t rangeLength) noexcept {
		const T *data = body.data() + position;
		if (position < part1Length) {
			if ((position + rangeLength) > part1Length) {
				// Range overlaps gap, so move gap to start of range.
				GapTo(position);
				data += gapLength;
			}
		} else {
			data += gapLength;
		}
		return data;
	}

	/// Return the position of the gap within the buffer.
	ptrdiff_t GapPosition() const noexcept {
		return part1Length;
	}
};

}
