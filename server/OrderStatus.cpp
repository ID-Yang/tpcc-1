/*
 * (C) Copyright 2015 ETH Zurich Systems Group (http://www.systems.ethz.ch/) and others.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Contributors:
 *     Markus Pilman <mpilman@inf.ethz.ch>
 *     Simon Loesing <sloesing@inf.ethz.ch>
 *     Thomas Etter <etterth@gmail.com>
 *     Kevin Bocksrocker <kevin.bocksrocker@gmail.com>
 *     Lucas Braun <braunl@inf.ethz.ch>
 */
#include "Transactions.hpp"
#include <telldb/Exceptions.hpp>

using namespace tell::db;

namespace tpcc {

OrderStatusResult Transactions::orderStatus(Transaction& tx, const OrderStatusIn& in) {
    OrderStatusResult result;
    try {
        auto oTableF = tx.openTable("order");
        auto olTableF = tx.openTable("order-line");
        auto cTableF = tx.openTable("customer");
        auto cTable = cTableF.get();
        auto oTable = oTableF.get();
        auto olTable = olTableF.get();
        // get Customer
        CustomerKey cKey{0, 0, 0};
        auto customerF = getCustomer(tx, in.selectByLastName, in.c_last, in.w_id, in.d_id, cTable, cKey);
        // get newest order
        auto iter = tx.reverse_lower_bound(oTable, "order_idx", {
                Field::create(in.w_id)
                , Field::create(in.d_id)
                , Field::create(cKey.c_id)
                , Field::create(nullptr)
                });
        OrderKey oKey{iter.value()};
        auto orderF = tx.get(oTable, iter.value());
        auto order = orderF.get();
        auto customer = customerF.get();
        // To get the order lines, we could use an index - but this is not necessary,
        // since we can generate all primary keys instead
        OrderlineKey olKey{in.w_id, in.d_id, oKey.o_id, int16_t(1)};
        try {
            std::vector<Future<Tuple>> reqs;
            while (true) {
                // to speed things up a bit, we always send 10 requests per iteration
                reqs.reserve(10);
                for (int i = 1; i <= 10; ++i) {
                    reqs.emplace_back(tx.get(olTable, olKey.key()));
                    olKey.ol_number += i;
                }
                for (auto& f : reqs) {
                    f.get();
                }
                reqs.clear();
            }
        } catch (tell::db::TupleDoesNotExist&) {
            // we got all order-lines
        }
        tx.commit();
        result.success = true;
    } catch (std::exception& ex) {
        result.success = false;
        result.error = ex.what();
    }
    return result;
}

} // namespace tpcc

