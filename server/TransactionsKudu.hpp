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
#pragma once
#include <common/Protocol.hpp>
#include <common/Util.hpp>
#include <kudu/client/client.h>

namespace tpcc {

class Transactions {
    int16_t mNumWarehouses;
    Random_t& rnd;
public:
    Transactions(int16_t numWarehouses) : mNumWarehouses(numWarehouses), rnd(*Random()) {}
public:
    NewOrderResult newOrderTransaction(kudu::client::KuduSession& session, const NewOrderIn& in);
    PaymentResult payment(kudu::client::KuduSession& session, const PaymentIn& in);
    OrderStatusResult orderStatus(kudu::client::KuduSession& session, const OrderStatusIn& in);
    DeliveryResult delivery(kudu::client::KuduSession& session, const DeliveryIn& in);
    StockLevelResult stockLevel(kudu::client::KuduSession& session, const StockLevelIn& in);
private:
    //tell::db::Future<tell::db::Tuple> getCustomer(kudu::client::KuduSession& tx,
    //        bool selectByLastName,
    //        const crossbow::string& c_last,
    //        int16_t c_w_id,
    //        int16_t c_d_id, int32_t c_id,
    //        tell::db::table_t customerTable,
    //        CustomerKey& customerKey);
};

} // namespace tpcc
