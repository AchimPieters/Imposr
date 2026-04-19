import { Router } from 'express';
import { ImposeController } from '../controllers/ImposeController';

const router = Router();
const controller = new ImposeController();

router.post('/', (req, res, next) => {
  void controller.impose(req, res, next);
});

export default router;
