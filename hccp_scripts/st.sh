#变量命名修改
# python3 st_pro_pro.py rdma_service/  --r --local-only -c rs.json --filter onlyc
# python3 st_pro_pro.py rdma_agent/client  --r --local-only -c ra.json --filter onlyc
# python3 st_pro_pro.py rdma_agent/peer  --r --local-only -c ra_peer.json --filter onlyc
# python3 st_pro_pro.py rdma_agent/hdc  --r --local-only -c ra_hdc.json --filter onlyc
# python3 st_pro_pro.py rdma_agent/adapter  --r --local-only -c ra_adp.json --filter onlyc
# python3 st_pro_pro.py inc/  --r --local-only -c rs.json  --filter onlyc
# python3 st_pro_pro.py common/  --r --local-only -c rs.json  --filter onlyc
# python3 st_pro_pro.py rdma_agent/comm  --r --local-only -c ra.json --filter onlyc
# python3 st_pro_pro.py hccp_service  --r --local-only -c hccp_service.json --filter onlyc
# python3 st_pro_pro.py rdma_service/  --r --local-only -c rs.json --filter onlyh
# python3 st_pro_pro.py rdma_agent/client  --r --local-only -c ra.json --filter onlyh
# python3 st_pro_pro.py rdma_agent/peer  --r --local-only -c ra_peer.json --filter onlyh
# python3 st_pro_pro.py rdma_agent/hdc  --r --local-only -c ra_hdc.json --filter onlyh
# python3 st_pro_pro.py rdma_agent/adapter  --r --local-only -c ra_adp.json --filter onlyh
# python3 st_pro_pro.py inc/  --r --local-only -c rs.json  --filter onlyh
# python3 st_pro_pro.py common/  --r --local-only -c rs.json  --filter onlyh
# python3 st_pro_pro.py rdma_agent/comm  --r --local-only -c ra.json --filter onlyh
# python3 st_pro_pro.py rdma_agent/inc/  --r --local-only -c rs.json --filter onlyh
# python3 st_pro_pro.py hccp_service  --r --local-only -c hccp_service.json --filter onlyh
python3 compile_test.py rs.json
python3 compile_test.py ra.json
python3 compile_test.py ra_peer.json
python3 compile_test.py ra_hdc.json
python3 compile_test.py ra_adp.json
python3 compile_test.py hccp_service.json


# python3 st_pro_pro.py rdma_service/  --r --filter onlyc -c rs.json
# python3 st_pro_pro.py rdma_service/ inc/ common/  --r --filter onlyh -c rs.json
# python3 compile_test.py rs.json

# rs 2 手动即可