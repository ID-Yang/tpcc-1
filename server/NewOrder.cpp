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
#include "CreateSchema.hpp"
#include <common/Util.hpp>

using namespace tell::db;

namespace tpcc {

namespace {

struct NewStock {
    int32_t s_quantity;
    int32_t s_ytd;
    int16_t s_order_cnt;
    int16_t s_remote_cnt;
};

}

NewOrderResult Transactions::newOrderTransaction(tell::db::Transaction& tx, const NewOrderIn& in)
{
    auto w_id = in.w_id;
    auto d_id = in.d_id;
    auto c_id = in.c_id;
    NewOrderResult result;
    try {
        Random rnd;
        int16_t o_all_local = 1;
        int16_t o_ol_cnt = rnd->randomWithin<int16_t>(5, 15);
        std::vector<int16_t> ol_supply_w_id(o_ol_cnt);
        for (auto& i : ol_supply_w_id) {
            i = w_id;
            if (rnd->randomWithin<int>(1, 100) == 1) {
                o_all_local = 0;
                while (i == w_id) {
                    i = rnd->randomWithin<int16_t>(1, mNumWarehouses);
                }
            }
        }
        auto datetime = now();
        auto wTableF = tx.openTable("warehouse");
        auto cTableF = tx.openTable("customer");
        auto dTableF = tx.openTable("district");
        auto oTableF = tx.openTable("order");
        auto noTableF = tx.openTable("new-order");
        auto iTableF = tx.openTable("item");
        auto olTableF = tx.openTable("order-line");
        auto sTableF = tx.openTable("stock");
        auto sTable = sTableF.get();
        auto olTable = olTableF.get();
        auto noTable = noTableF.get();
        auto oTable = oTableF.get();
        auto dTable = dTableF.get();
        auto cTable = cTableF.get();
        auto wTable = wTableF.get();
        auto iTable = iTableF.get();
        WarehouseKey wKey(w_id);
        CustomerKey cKey(w_id, d_id, c_id);
        DistrictKey dKey(w_id, d_id);
        auto warehouseF = tx.get(wTable, wKey.key());
        auto customerF = tx.get(cTable, cKey.key());
        auto districtF = tx.get(dTable, dKey.key());
        auto district = districtF.get();
        auto customer = customerF.get();
        auto warehouse = warehouseF.get();
        auto districtNew = district;
        auto d_next_o_id = district.at("d_next_o_id");
        districtNew.at("d_next_o_id") += Field::create(int32_t(1));
        tx.update(dTable, dKey.key(), district, districtNew);
        // insert order
        auto o_id = boost::any_cast<int32_t>(d_next_o_id.value());
        OrderKey oKey(w_id, d_id, o_id);
        auto order = std::make_tuple(o_id, d_id, w_id, c_id, datetime, nullptr, o_ol_cnt, o_all_local);
        tx.insert(oTable, oKey.key(), order);
        // insert new-order
        tx.insert(noTable, oKey.key(), std::make_tuple(o_id, d_id, w_id));
        // generate random items
        std::vector<int16_t> ol_i_id;
        for (int16_t i = 0; i < o_ol_cnt; ++i) {
            auto i_id = rnd->NURand<int32_t>(8191,1,100000);
            ol_i_id.push_back(i_id);
        }
        // get the items
        // get the stocks
        ol_i_id.reserve(o_ol_cnt);
        std::unordered_map<int16_t, Future<Tuple>> itemsF;
        std::unordered_map<int16_t, Tuple> items;
        std::unordered_map<std::pair<int16_t, int16_t>, Future<Tuple>> stocksF;
        std::unordered_map<std::pair<int16_t, int16_t>, Tuple> stocks;
        itemsF.reserve(o_ol_cnt);
        items.reserve(o_ol_cnt);
        stocksF.reserve(o_ol_cnt);
        stocks.reserve(o_ol_cnt);
        for (int16_t i = 0; i < o_ol_cnt; ++i) {
            auto i_id = ol_i_id[i];
            auto supply_w_id = ol_supply_w_id[i];
            if (itemsF.count(i_id) == 0) {
                itemsF.emplace(i_id, tx.get(iTable, tell::db::key_t{uint64_t(i_id)}));
            }
            if (stocksF.count(std::make_pair(i_id, supply_w_id)) == 0) {
                StockKey sKey(supply_w_id, i_id);
                stocksF.emplace(std::make_pair(i_id, supply_w_id), tx.get(sTable, sKey.key()));
            }
        }
        std::unordered_map<std::pair<int16_t, int16_t>, NewStock> newStocks;
        for (auto& p : stocksF) {
            auto stock = p.second.get();
            NewStock nStock;
            nStock.s_quantity = boost::any_cast<int32_t>(stock.at("s_quantity").value());
            nStock.s_ytd = boost::any_cast<int32_t>(stock.at("s_ytd").value());
            nStock.s_order_cnt = boost::any_cast<int16_t>(stock.at("s_order_cnt").value());
            nStock.s_remote_cnt = boost::any_cast<int16_t>(stock.at("s_remote_cnt").value());
            newStocks.emplace(p.first, std::move(nStock));
            stocks.emplace(p.first, std::move(stock));
        }
        for (auto& p : itemsF) {
            items.emplace(p.first, p.second.get());
        }
        // set ol_dist_info key
        crossbow::string ol_dist_info_key;
        if (d_id == 10) {
            ol_dist_info_key ="s_dist_10";
        } else {
            ol_dist_info_key = "s_dist_0" + crossbow::to_string(d_id);
        }
        int32_t ol_amount_sum = 0;
        // insert the order lines
        for (int16_t ol_number = 1; ol_number <= o_ol_cnt; ++ol_number) {
            auto& item = items.at(ol_number);
            auto stockId = std::make_pair(ol_i_id[ol_number], ol_supply_w_id[ol_number]);
            auto& stock = stocks.at(stockId);
            auto ol_dist_info = boost::any_cast<crossbow::string>(stock.at(ol_dist_info_key));
            auto ol_quantity = rnd->randomWithin<int16_t>(1, 10);
            auto& newStock = newStocks.at(stockId);
            if (newStock.s_quantity > ol_quantity + 10) {
                newStock.s_quantity -= ol_quantity;
            } else {
                newStock.s_quantity = (newStock.s_quantity - ol_quantity) + 91;
            }
            newStock.s_ytd += ol_quantity;
            ++newStock.s_order_cnt;
            if (ol_supply_w_id[ol_number] != w_id) ++newStock.s_remote_cnt;
            auto i_price = boost::any_cast<int32_t>(item.at("i_price").value());
            int32_t ol_amount = i_price * int32_t(ol_quantity);
            ol_amount_sum += ol_amount;
            OrderlineKey olKey(w_id, d_id, o_id, ol_number);
            tx.insert(olTable, olKey.key(),
                    std::make_tuple(o_id, d_id, w_id, ol_number, ol_i_id[ol_number],
                        ol_supply_w_id[ol_number], nullptr, ol_quantity, ol_amount, ol_dist_info));
            // set Result for this order line
            const auto& i_data = boost::any_cast<const crossbow::string&>(item.at("i_data").value());
            const auto& s_data = boost::any_cast<const crossbow::string&>(stock.at("s_data").value());
            NewOrderResult::OrderLine lineRes;
            lineRes.ol_supply_w_id = ol_supply_w_id[ol_number];
            lineRes.ol_i_id = ol_i_id[ol_number];
            lineRes.i_name = boost::any_cast<crossbow::string>(item.at("i_name").value());
            lineRes.ol_quantity = ol_quantity;
            lineRes.s_quantity = newStock.s_quantity;
            lineRes.i_price = i_price;
            lineRes.ol_amount = ol_amount;
            lineRes.brand_generic = 'G';
            if (i_data.find("ORIGINAL") != i_data.npos && s_data.find("ORIGINAL") != s_data.npos) {
                lineRes.brand_generic = 'B';
            }
            result.lines.emplace_back(std::move(lineRes));
        }
        // update stock-entries
        for (const auto& p : stocks) {
            StockKey sKey(p.first.second, p.first.first);
            const auto& nStock = newStocks.at(p.first);
            auto n = p.second;
            n.at("s_quantity") = Field::create(nStock.s_quantity);
            n.at("s_ytd") = Field::create(nStock.s_quantity);
            n.at("s_order_cnt") = Field::create(nStock.s_quantity);
            n.at("s_remote_cnt") = Field::create(nStock.s_quantity);
            tx.update(sTable, sKey.key(), p.second, n);
        }
        // 1% of transactions need to abort
        if (rnd->randomWithin<int>(1, 100) == 1) {
            tx.rollback();
            result.success = false;
            result.error = "Item number is not valid";
            result.lines.clear();
        } else {
            // write single-line results
            result.o_id = o_id;
            result.o_ol_cnt = o_ol_cnt;
            result.c_last = boost::any_cast<crossbow::string>(customer.at("c_last").value());
            result.c_credit = boost::any_cast<int16_t>(customer.at("c_credit").value());
            result.c_discount = boost::any_cast<int32_t>(customer.at("c_discount").value());
            result.w_tax = boost::any_cast<int32_t>(warehouse.at("w_tax").value());
            result.d_tax = boost::any_cast<int32_t>(district.at("d_tax").value());
            result.o_entry_d = datetime;
            result.total_amount = ol_amount_sum * (1 - result.c_discount) * (1 + result.w_tax + result.d_tax);
            tx.commit();
        }
    } catch (std::exception& ex) {
        result.success = false;
        result.error = ex.what();
        result.lines.clear();
    }
    return result;
}

} // namespace tpcc
