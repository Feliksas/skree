#pragma once

#include <memory>
#include <deque>

namespace Skree {
    namespace Utils {
        struct muh_str_t;

        class StringSequence {
        public:
            struct TItem {
                size_t len;
                const char* data;
            };

        private:
            std::deque<TItem> sequence;
            std::deque<std::shared_ptr<muh_str_t>> memory;

        public:
            inline void concat(size_t _len, const char* _data) {
                sequence.push_back(TItem {
                    .len = _len,
                    .data = _data
                });
            }

            void concat(const std::shared_ptr<muh_str_t>& data);

            inline StringSequence() {
            }

            inline StringSequence(size_t _len, const char* _data) {
                concat(_len, _data);
            }

            template<typename T>
            inline const char* read(
                size_t index,
                size_t offset = 0,
                T* _len = nullptr,
                size_t* _next = nullptr
            ) const {
                // Utils::cluck(4, "read(%lu, %lu, ...), sequence.size() == %lu", index, offset, sequence.size());

                if(index >= sequence.size())
                    throw std::logic_error ("Reading past the end of string sequence (1)");

                const TItem* node = &sequence[index];

                while(offset >= node->len) {
                    if(++index >= sequence.size()) {
                        throw std::logic_error ("Reading past the end of string sequence (2)");

                    } else {
                        offset -= node->len;
                        node = &sequence[index];
                    }
                }

                if(_len != nullptr)
                    *_len = node->len - offset;

                if(_next != nullptr)
                    *_next = index;

                return node->data + offset;
            }

            template<typename T>
            inline const char* read(
                size_t index,
                size_t offset = 0,
                size_t* _next = nullptr,
                T* _len = nullptr
            ) const {
                return read(index, offset, _len, _next);
            }

            inline decltype(sequence)::const_iterator begin() const {
                return sequence.begin();
            }

            inline decltype(sequence)::const_iterator end() const {
                return sequence.end();
            }
        };
    }
}
